//===-- Z80LegalizerInfo.cpp - Z80 Legalizer -----------------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the interface that Z80 uses to legalize generic MIR.
//
// The Z80 is an 8-bit processor with some 16-bit operations. The legalizer
// needs to handle breaking down larger operations into sequences of 8-bit
// and 16-bit operations.
//
//===----------------------------------------------------------------------===//

#include "Z80LegalizerInfo.h"

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80MachineFunctionInfo.h"
#include "Z80Subtarget.h"

#include "llvm/CodeGen/GlobalISel/GenericMachineInstrs.h"
#include "llvm/CodeGen/GlobalISel/LegalizerHelper.h"
#include "llvm/CodeGen/GlobalISel/MachineIRBuilder.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/TargetOpcodes.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IntrinsicsZ80.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/WithColor.h"

using namespace llvm;

/// Check if all three fast-math flags (nnan, ninf, nsz) are set.
/// If only some are set, emit a one-time remark so the user knows why the
/// fast soft-float path was not selected.
static bool hasAllFastFlags(const MachineInstr &MI,
                            MachineIRBuilder &MIRBuilder) {
  bool NoNans = MI.getFlag(MachineInstr::FmNoNans);
  bool NoInfs = MI.getFlag(MachineInstr::FmNoInfs);
  bool Nsz = MI.getFlag(MachineInstr::FmNsz);
  if (NoNans && NoInfs && Nsz)
    return true;
  if (NoNans || NoInfs || Nsz) {
    static bool Warned = false;
    if (!Warned) {
      Warned = true;
      WithColor::warning() << "partial fast-math flags (have:";
      auto &OS = errs();
      if (NoNans)
        OS << " nnan";
      if (NoInfs)
        OS << " ninf";
      if (Nsz)
        OS << " nsz";
      OS << ", missing:";
      if (!NoNans)
        OS << " nnan";
      if (!NoInfs)
        OS << " ninf";
      if (!Nsz)
        OS << " nsz";
      OS << ") - need all three for fast soft-float path\n";
    }
  }
  return false;
}

Z80LegalizerInfo::Z80LegalizerInfo(const Z80Subtarget &STI) {
  using namespace TargetOpcode;

  const LLT S1 = LLT::scalar(1);
  const LLT S8 = LLT::scalar(8);
  const LLT S16 = LLT::scalar(16);
  const LLT S32 = LLT::scalar(32);
  const LLT S64 = LLT::scalar(64);
  const LLT S128 = LLT::scalar(128);
  const LLT P0 = LLT::pointer(0, 16); // Default address space, 16-bit pointers

  // Basic type legalization for Z80
  // Most operations need to be broken down to 8-bit or 16-bit
  // 32-bit operations are narrowed to pairs of 16-bit operations

  // Constants - clamp to legal sizes, narrow larger
  getActionDefinitionsBuilder(G_CONSTANT)
      .legalFor({S8, S16, P0})
      .widenScalarToNextPow2(0, 8)
      .clampScalar(0, S8, S16);

  // Frame index
  getActionDefinitionsBuilder(G_FRAME_INDEX).legalFor({P0});

  // Global values and block addresses
  getActionDefinitionsBuilder({G_GLOBAL_VALUE, G_BLOCK_ADDR}).legalFor({P0});

  // Integer extension/truncation
  // s1->s8 is a no-op (s1 already lives in an 8-bit register on Z80)
  getActionDefinitionsBuilder({G_ANYEXT, G_SEXT, G_ZEXT})
      .legalFor({{S16, S8}, {S8, S1}, {S16, S1}})
      .widenScalarToNextPow2(1)
      .clampScalar(0, S8, S16);

  // s1 lives in GR8 on Z80, so s1 is valid as a trunc destination.
  getActionDefinitionsBuilder(G_TRUNC)
      .legalFor({{S8, S16}, {S1, S8}, {S1, S16}})
      .widenScalarToNextPow2(1)
      .clampScalar(1, S8, S16);

  // Basic ALU operations
  // For 32-bit and larger, narrow to 16-bit which generates carry chains
  // widenScalarToNextPow2 ensures non-pow2 types (e.g. s63 from f64 bit
  // manipulation in Rust core) are widened before narrowScalar operates.
  getActionDefinitionsBuilder({G_ADD, G_SUB})
      .legalFor({S8, S16})
      .widenScalarToNextPow2(0)
      .narrowScalar(0, LegalizeMutations::changeTo(0, S16))
      .clampScalar(0, S8, S16);

  // Carry-chain operations for multi-precision arithmetic
  // G_UADDO: unsigned add with overflow (returns result + overflow flag)
  // G_UADDE: unsigned add with carry in (for chaining)
  // These are used when narrowing 32-bit+ operations
  getActionDefinitionsBuilder({G_UADDO, G_SADDO})
      .legalFor({{S16, S1}})
      .clampScalar(0, S16, S16)
      .minScalar(1, S1);

  getActionDefinitionsBuilder(G_UADDE)
      .legalFor({{S16, S1}})
      .clampScalar(0, S16, S16)
      .minScalar(2, S1);

  // G_SADDE: lower to add + XOR-based overflow detection.
  // Unlike G_UADDE which uses hardware carry, signed overflow needs arithmetic.
  getActionDefinitionsBuilder(G_SADDE).lower();

  getActionDefinitionsBuilder({G_USUBO, G_SSUBO})
      .legalFor({{S16, S1}})
      .clampScalar(0, S16, S16)
      .minScalar(1, S1);

  getActionDefinitionsBuilder(G_USUBE)
      .legalFor({{S16, S1}})
      .clampScalar(0, S16, S16)
      .minScalar(2, S1);

  // G_SSUBE: lower to sub + XOR-based overflow detection.
  getActionDefinitionsBuilder(G_SSUBE).lower();

  getActionDefinitionsBuilder({G_AND, G_OR, G_XOR})
      .legalFor({S8})
      .widenScalarToNextPow2(0)
      .clampScalar(0, S8, S8);

  // Shifts - 8-bit shifts are native, 16-bit left shift uses ADD HL,HL
  // clampScalar(1) must come BEFORE clampScalar(0): when narrowing i64
  // shifts, narrowScalarShift generates comparisons using the shift amount's
  // type. If the amount is still s64, these become i64 comparisons with
  // massive register pressure. Narrowing the amount to s8 first ensures
  // comparisons are simple i8 operations.
  // Non-power-of-2 types (e.g. s17 from SCEV closed-form sum) are caught
  // by customIf and widened to next pow2 in legalizeCustom before the
  // standard narrowScalar path handles them.
  auto isNonPow2ShiftType = [=](const LegalityQuery &Q) {
    unsigned Size = Q.Types[0].getSizeInBits();
    return Size > 1 && !isPowerOf2_32(Size);
  };
  getActionDefinitionsBuilder(G_SHL)
      .legalFor({{S8, S8}, {S16, S8}})
      .customIf(isNonPow2ShiftType)
      .clampScalar(1, S8, S8)
      .clampScalar(0, S8, S16);

  getActionDefinitionsBuilder({G_LSHR, G_ASHR})
      .legalFor({{S8, S8}, {S16, S8}})
      .customIf(isNonPow2ShiftType)
      .clampScalar(1, S8, S8)
      .clampScalar(0, S8, S16);

  // Multiply - Z80 has no hardware multiply
  // i8:  legal (handled in instruction selector via zero-extend + __mulhi3)
  // i16: legal (handled in instruction selector via call to __mulhi3)
  // i32: libcall (__mulsi3) — avoids narrowing to 4 separate i16 multiplies
  getActionDefinitionsBuilder(G_MUL)
      .legalFor({S8, S16})
      .libcallFor({S32, S64})
      .narrowScalarIf(LegalityPredicates::typeIs(0, S128),
                      LegalizeMutations::changeTo(0, S64))
      .widenScalarToNextPow2(0)
      .clampScalar(0, S8, S64);

  // G_UMULH - Upper half of multiply (needed when narrowing s32 mul)
  // s16: legal (native __mulhi3-based)
  // s32+: narrow to s16 (avoid lower→mul 2x→narrow→G_UMULH recursion)
  // s8: lower to zext s16 + mul s16 + shift + trunc
  getActionDefinitionsBuilder(G_UMULH)
      .legalFor({S16})
      .maxScalar(0, S16)
      .lower();

  // G_SMULH - Upper half of signed 16x16 multiply
  // Custom: smulh(a,b) = umulh(a,b) - (a>>15)&b - (b>>15)&a
  // Uses 1 umulh call instead of the default lowering's 4 multiply calls.
  getActionDefinitionsBuilder(G_SMULH).customFor({S16}).lower();

  // Multiply with overflow detection
  // G_UMULO: lower to G_MUL + G_UMULH, overflow = (umulh != 0)
  // G_SMULO: lower to G_MUL + G_SMULH, overflow = (smulh != sign_ext(result))
  getActionDefinitionsBuilder({G_UMULO, G_SMULO}).lower();

  // Division - Z80 has no hardware divide
  // i8: legal (inline DJNZ loop in instruction selector)
  // i16: legal (handled in instruction selector via call)
  // i32: libcall (__divsi3, __udivsi3, __modsi3, __umodsi3)
  // i64: libcall (__divdi3, __udivdi3, __moddi3, __umoddi3)
  // i128: libcall (__divti3, __udivti3, __modti3, __umodti3)
  getActionDefinitionsBuilder({G_UDIV, G_UREM, G_SDIV, G_SREM})
      .legalFor({S8, S16})
      .libcallFor({S32, S64, S128})
      .widenScalarToNextPow2(0)
      .clampScalar(0, S8, S128);

  // Combined div+rem: lower back to separate G_UDIV/G_UREM (or G_SDIV/G_SREM).
  // The div_rem_to_divrem combine creates these, but Z80 has no fused divrem.
  getActionDefinitionsBuilder({G_UDIVREM, G_SDIVREM}).lower();

  // Comparisons
  // G_ICMP produces a boolean result - we widen it to S8 since Z80 has
  // no 1-bit registers. The result will be 0 or 1.
  // Note: s1 IS a power of 2, so widenScalarToNextPow2 won't widen it.
  // We must use minScalar to force s1 -> s8.
  getActionDefinitionsBuilder(G_ICMP)
      .legalFor({{S8, S8}, {S8, S16}, {S8, P0}})
      .customFor({{S8, S32}, {S8, S64}, {S8, S128}})
      .minScalar(0, S8)
      .widenScalarToNextPow2(1)
      .clampScalar(1, S8, S128);

  // Select
  // G_SELECT condition (operand 1) is s1, which needs widening to s8
  getActionDefinitionsBuilder(G_SELECT)
      .legalFor({{S8, S8}, {S16, S8}, {P0, S8}})
      .minScalar(1, S8)
      .widenScalarToNextPow2(0)
      .clampScalar(0, S8, S16);

  // Memory operations
  // The pre-legalization combiner can fold G_ANYEXT(G_LOAD s8) into a
  // G_LOAD with result s16 and memory s8.  Custom-lower extending loads
  // (result type > memory type) back to G_LOAD + G_ANYEXT, since the
  // generic LegalizerHelper::lowerLoad does not handle this case.
  // G_LOAD and G_STORE are defined separately because the extending load
  // customIf must not apply to G_STORE.
  getActionDefinitionsBuilder(G_LOAD)
      .legalForTypesWithMemDesc(
          {{S8, P0, S8, 1}, {S16, P0, S16, 1}, {P0, P0, S16, 1}})
      .lowerIfMemSizeNotByteSizePow2()
      .customIf([](const LegalityQuery &Q) {
        return Q.Types[0].getSizeInBits() >
               Q.MMODescrs[0].MemoryTy.getSizeInBits();
      })
      .clampScalar(0, S8, S16);

  getActionDefinitionsBuilder(G_STORE)
      .legalForTypesWithMemDesc(
          {{S8, P0, S8, 1}, {S16, P0, S16, 1}, {P0, P0, S16, 1}})
      .lowerIfMemSizeNotByteSizePow2()
      .clampScalar(0, S8, S16);

  // Pointer operations
  getActionDefinitionsBuilder(G_PTR_ADD).legalFor({{P0, S16}});

  // Pointer/integer conversions - no-op on Z80 (both are 16-bit)
  // Wider integers (e.g. s32 from GEP with i32 index) are narrowed to s16.
  getActionDefinitionsBuilder(G_INTTOPTR)
      .legalFor({{P0, S16}})
      .clampScalar(1, S16, S16);
  getActionDefinitionsBuilder(G_PTRTOINT).legalFor({{S16, P0}});

  // Bitcast - no-op reinterpretation between same-size types
  getActionDefinitionsBuilder(G_BITCAST).legalFor({{S16, P0}, {P0, S16}});

  // Branches
  // G_BRCOND takes a boolean condition - we use S8 since Z80 has no 1-bit regs.
  // Non-zero means branch taken.
  // Note: s1 IS a power of 2, so widenScalarToNextPow2 won't widen it.
  getActionDefinitionsBuilder(G_BRCOND).legalFor({S8}).minScalar(0, S8);
  getActionDefinitionsBuilder(G_BR).legalIf(
      [](const LegalityQuery &) { return true; });

  // Jump table support
  getActionDefinitionsBuilder(G_BRJT).customFor({{P0, S16}});
  getActionDefinitionsBuilder(G_BRINDIRECT).legalFor({P0});
  getActionDefinitionsBuilder(G_JUMP_TABLE).legalFor({P0});
  getActionDefinitionsBuilder(G_TRAP).legalIf(
      [](const LegalityQuery &) { return true; });

  // PHI nodes
  getActionDefinitionsBuilder(G_PHI)
      .legalFor({S8, S16, P0})
      .widenScalarToNextPow2(0)
      .clampScalar(0, S8, S16);

  // Freeze - converts undef to a deterministic value. No-op at codegen level.
  getActionDefinitionsBuilder(G_FREEZE)
      .legalFor({S8, S16, P0})
      .widenScalarToNextPow2(0)
      .clampScalar(0, S8, S16);

  // Copy
  getActionDefinitionsBuilder(G_IMPLICIT_DEF)
      .legalFor({S8, S16, P0})
      .widenScalarToNextPow2(0)
      .clampScalar(0, S8, S16);

  // Merge/unmerge for extracting/combining values
  // S8 from S16, S16 from S32
  getActionDefinitionsBuilder(G_UNMERGE_VALUES)
      .legalFor({{S8, S16}, {S16, S32}});

  getActionDefinitionsBuilder(G_MERGE_VALUES)
      .legalFor({{S16, S8}, {S32, S16}})
      .lower();

  // Build vector (for creating 16-bit from two 8-bit values)
  getActionDefinitionsBuilder(G_BUILD_VECTOR).legalFor({{S16, S8}});

  // Bit manipulation builtins
  // G_BSWAP: custom lowering using UNMERGE+MERGE to avoid CSE conflict
  // when generic lowerBswap constant-folds and creates duplicate constants
  getActionDefinitionsBuilder(G_BSWAP).customFor({S16}).lower();
  getActionDefinitionsBuilder({G_BITREVERSE, G_CTPOP}).lower();
  getActionDefinitionsBuilder(
      {G_CTLZ, G_CTLZ_ZERO_UNDEF, G_CTTZ, G_CTTZ_ZERO_UNDEF})
      .lower();

  // Extending loads - custom lower to load + extend.
  // The generic .lower() path (lowerLoad) refuses to split extending loads when
  // the memory access is natively supported (power-of-2 and aligned), because
  // it's designed for splitting unaligned/non-pow2 memory accesses.  We need
  // custom lowering to decompose G_ZEXTLOAD/G_SEXTLOAD → G_LOAD +
  // G_ZEXT/G_SEXT.
  getActionDefinitionsBuilder({G_SEXTLOAD, G_ZEXTLOAD}).custom();

  // Dynamic stack allocation (alloca/VLA) - custom lowering to avoid
  // PTRTOINT/INTTOPTR COPY chain that loses register class constraints
  getActionDefinitionsBuilder(G_DYN_STACKALLOC).custom();

  // Stack save/restore for VLA scope cleanup
  getActionDefinitionsBuilder({G_STACKSAVE, G_STACKRESTORE}).custom();

  // 8-bit rotations use native Z80 rotate instructions (RLCA/RRCA).
  // Wider rotations are lowered to shift + or sequences.
  // The pre-legalize combiner converts G_FSHL(x,x,n) → G_ROTL(x,n).
  getActionDefinitionsBuilder({G_ROTL, G_ROTR}).legalFor({S8}).lower();

  // Funnel shifts - lower to shift + or sequences.
  getActionDefinitionsBuilder({G_FSHL, G_FSHR}).lower();

  // Sign extend in register - width 8 is handled by instruction selector
  // (SEXT_GR8_GR16 pseudo), other widths lowered to SHL+ASHR in custom.
  getActionDefinitionsBuilder(G_SEXT_INREG).customFor({S16}).lower();

  // Variadic function support
  getActionDefinitionsBuilder(G_VASTART).customFor({P0});
  getActionDefinitionsBuilder(G_VAARG).lower();

  // Saturating arithmetic
  // i8: legal (expanded to flag-based pseudo in ISel/ExpandPseudo)
  // i16+: lower to add/sub + overflow check + select
  // SM83 has no P/V overflow flag, so signed i8 sat must also be lowered there.
  getActionDefinitionsBuilder({G_UADDSAT, G_USUBSAT}).legalFor({S8}).lower();
  if (STI.hasSM83()) {
    getActionDefinitionsBuilder({G_SADDSAT, G_SSUBSAT}).lower();
  } else {
    getActionDefinitionsBuilder({G_SADDSAT, G_SSUBSAT}).legalFor({S8}).lower();
  }

  // Three-way comparison - lower to two comparisons
  getActionDefinitionsBuilder({G_SCMP, G_UCMP}).lower();

  // Min/max/abs - lower to comparison + select sequences
  getActionDefinitionsBuilder({G_UMAX, G_UMIN, G_SMAX, G_SMIN}).lower();

  getActionDefinitionsBuilder(G_ABS).lower();

  // Memory intrinsics - lower to runtime library calls
  getActionDefinitionsBuilder({G_MEMCPY, G_MEMMOVE}).libcall();

  // G_MEMSET needs custom handling: promote i8 val to i16 (C 'int')
  // before lowering to libcall, so calling convention assigns it correctly
  getActionDefinitionsBuilder(G_MEMSET).custom();

  // ===== Floating point (softfloat — all via library calls) =====
  // Z80 has no FPU. All float operations are lowered to calls to
  // GCC-standard softfloat library functions.
  //
  // f32 (float): fully implemented — runtime in z80_rt.asm provides
  //   __addsf3, __subsf3, __mulsf3, __divsf3, __gesf2, etc.
  //   f16 operations are widened to f32.
  //
  // f64 (double): NOT implemented at runtime. Legalization emits standard
  //   GCC libcall names (__adddf3, __fixdfdi, etc.) so compilation succeeds,
  //   but linking will fail with undefined symbols unless a double-precision
  //   softfloat library is provided. This allows code that *references* double
  //   (e.g. Rust's core::num) to compile as long as the double paths are not
  //   actually reached at link time (LTO/DCE can eliminate them).

  // Arithmetic: f32 → custom (selects fast-math or IEEE libcall)
  // When fast-math flags (nnan) are set, calls __addsf3_fast etc.
  // which skip NaN/Inf/zero special-case checks for ~20% speedup.
  // f64 → libcall (__adddf3, __subdf3, __muldf3, __divdf3) — unimplemented
  getActionDefinitionsBuilder({G_FADD, G_FSUB, G_FMUL, G_FDIV})
      .customFor({S32})
      .libcallFor({S64})
      .minScalar(0, S32);

  // Comparison: f32 → custom (returns i16 tri-state)
  // f64 → libcall (__gedf2, __ledf2, etc.) — unimplemented
  getActionDefinitionsBuilder(G_FCMP)
      .customForCartesianProduct({S1}, {S32})
      .libcallForCartesianProduct({S1}, {S64})
      .minScalar(1, S32);

  // Float constant → custom (bitcast FP bits to integer constant)
  getActionDefinitionsBuilder(G_FCONSTANT).customFor({S64, S32, S16});

  // Negation/Abs → lower (XOR/AND on sign bit — no libcall needed)
  getActionDefinitionsBuilder({G_FNEG, G_FABS}).lowerFor({S64, S32, S16});

  // Float width conversions:
  //   f16↔f32: libcall (__extendhfsf2, __truncsfhf2)
  //   f32↔f64: libcall (__extendsfdf2, __truncdfsf2) — unimplemented
  getActionDefinitionsBuilder(G_FPEXT).libcallFor({{S32, S16}, {S64, S32}});

  getActionDefinitionsBuilder(G_FPTRUNC).libcallFor({{S16, S32}, {S32, S64}});

  // Float↔int conversions:
  //   f32↔i32: libcall (__fixsfsi, __floatsisf, etc.)
  //   f64↔i32/i64: libcall (__fixdfsi, __fixdfdi, etc.) — unimplemented
  getActionDefinitionsBuilder({G_FPTOSI, G_FPTOUI})
      .libcallForCartesianProduct({S32, S64}, {S32, S64})
      .minScalar(0, S32)
      .minScalar(1, S32);

  getActionDefinitionsBuilder({G_SITOFP, G_UITOFP})
      .libcallForCartesianProduct({S32, S64}, {S32, S64})
      .minScalar(0, S32)
      .minScalar(1, S32);

  // Saturating float→int: lower to G_FPTOSI/G_FPTOUI + min/max clamp.
  // The lowered G_FPTOSI/G_FPTOUI will then hit the libcall rules above.
  getActionDefinitionsBuilder({G_FPTOSI_SAT, G_FPTOUI_SAT}).lower();

  getActionDefinitionsBuilder(G_IS_FPCLASS).lower();

  // FP math functions — all lowered via GCC-compatible libcalls.
  // f32 uses our runtime (__addsf3 etc.), f64 uses standard GCC names
  // (__adddf3 etc.) and requires an external soft-float library at link time.
  getActionDefinitionsBuilder(
      {G_FSQRT, G_FSIN, G_FCOS, G_FLOG, G_FLOG2, G_FLOG10, G_FEXP, G_FEXP2})
      .libcallFor({S32, S64});

  getActionDefinitionsBuilder({G_FREM, G_FPOW}).libcallFor({S32, S64});

  getActionDefinitionsBuilder({G_FMINNUM, G_FMAXNUM}).libcallFor({S32, S64});

  // FP rounding functions — libcalls for both f32 and f64.
  getActionDefinitionsBuilder({G_FFLOOR, G_FCEIL, G_FRINT, G_FNEARBYINT,
                               G_INTRINSIC_TRUNC, G_INTRINSIC_ROUND,
                               G_INTRINSIC_ROUNDEVEN})
      .libcallFor({S32, S64});

  // G_FMA: libcall (fmaf/fma). G_FMAD: lower to fmul+fadd.
  getActionDefinitionsBuilder(G_FMA).libcallFor({S32, S64});
  getActionDefinitionsBuilder(G_FMAD).lower();

  // G_FCOPYSIGN: lower to bit manipulation (extract sign bit, mask, OR).
  getActionDefinitionsBuilder(G_FCOPYSIGN).lower();

  // G_LROUND/G_LLROUND: float→int rounding via libcall.
  // Result is S32 (long = i32 on Z80). Input is S32 (float) or S64 (double).
  getActionDefinitionsBuilder({G_LROUND, G_LLROUND})
      .libcallForCartesianProduct({S32}, {S32, S64});

  // G_FENCE: no-op on single-threaded Z80 — erase in legalizeCustom.
  getActionDefinitionsBuilder(G_FENCE).custom();

  getLegacyLegalizerInfo().computeTables();
}

bool Z80LegalizerInfo::legalizeIntrinsic(LegalizerHelper &Helper,
                                         MachineInstr &MI) const {
  // Handle Z80-specific intrinsics
  unsigned IntrinsicID = cast<GIntrinsic>(MI).getIntrinsicID();

  switch (IntrinsicID) {
  case Intrinsic::z80_in:
  case Intrinsic::z80_out:
  case Intrinsic::z80_halt:
  case Intrinsic::z80_di:
  case Intrinsic::z80_ei:
  case Intrinsic::z80_nop:
    // These intrinsics are legal and will be selected directly
    return true;
  default:
    return false;
  }
}

bool Z80LegalizerInfo::legalizeCustom(LegalizerHelper &Helper, MachineInstr &MI,
                                      LostDebugLocObserver &LocObserver) const {
  MachineIRBuilder &MIRBuilder = Helper.MIRBuilder;
  MachineRegisterInfo &MRI = *MIRBuilder.getMRI();

  switch (MI.getOpcode()) {
  case TargetOpcode::G_FENCE:
    // Z80 is single-threaded with no memory reordering — fences are no-ops.
    MI.eraseFromParent();
    return true;

  case TargetOpcode::G_DYN_STACKALLOC: {
    // SM83 has no frame pointer register (no IX/IY), so dynamic stack
    // allocation is not supported — SP-relative offsets break when SP moves.
    if (MIRBuilder.getMF().getSubtarget<Z80Subtarget>().hasSM83())
      report_fatal_error(
          "SM83 does not support dynamic stack allocation "
          "(alloca/VLA) because it has no frame pointer register");

    // Custom lowering for dynamic stack allocation (alloca/VLA).
    // The generic lowering creates a COPY chain:
    //   COPY $sp (p0) → PTRTOINT (s16) → G_SUB → INTTOPTR (p0) → COPY $sp
    // The PTRTOINT intermediate causes a register class constraint loss when
    // COPY propagation folds the chain, leading to SP being copied to BC/DE
    // which Z80 hardware cannot do (only ADD HL,SP / ADD IX,SP / ADD IY,SP).
    //
    // Our custom lowering reads SP directly as s16, avoiding PTRTOINT:
    //   %sp_val:s16 = COPY $sp
    //   %new_sp:s16 = G_SUB %sp_val, %size
    //   $sp = COPY %new_sp
    //   %result:p0 = G_INTTOPTR %new_sp
    Register Dst = MI.getOperand(0).getReg();
    Register AllocSize = MI.getOperand(1).getReg();
    Align Alignment = assumeAligned(MI.getOperand(2).getImm());

    LLT S16 = LLT::scalar(16);

    // Read SP directly as integer (no p0 intermediate, no PTRTOINT needed)
    auto SPVal = MIRBuilder.buildCopy(S16, Register(Z80::SP));

    // Subtract allocation size
    auto NewSP = MIRBuilder.buildSub(S16, SPVal, AllocSize);

    // Handle alignment if needed
    if (Alignment > Align(1)) {
      APInt AlignMask(16, Alignment.value(), true);
      AlignMask.negate();
      auto AlignCst = MIRBuilder.buildConstant(S16, AlignMask);
      NewSP = MIRBuilder.buildAnd(S16, NewSP, AlignCst);
    }

    // Write new SP value back
    MIRBuilder.buildCopy(Register(Z80::SP), NewSP);

    // Result is the new SP value as a pointer
    MIRBuilder.buildIntToPtr(Dst, NewSP);

    MI.eraseFromParent();
    return true;
  }

  case TargetOpcode::G_BSWAP: {
    // Custom bswap i16: swap high and low bytes using UNMERGE+MERGE.
    // Avoids generic lowerBswap which uses setReg(Dst) on the result,
    // conflicting with CSE when constant folding produces duplicate constants.
    auto [Dst, Src] = MI.getFirst2Regs();
    LLT S8 = LLT::scalar(8);
    auto Unmerge = MIRBuilder.buildUnmerge(S8, Src);
    // Little-endian: element 0 = low byte, element 1 = high byte
    // Swap: new low = old high, new high = old low
    MIRBuilder.buildMergeValues(Dst, {Unmerge.getReg(1), Unmerge.getReg(0)});
    MI.eraseFromParent();
    return true;
  }

  case TargetOpcode::G_STACKSAVE: {
    // %dst:p0 = G_STACKSAVE → read SP as s16, convert to pointer
    Register Dst = MI.getOperand(0).getReg();
    LLT S16 = LLT::scalar(16);
    auto SPVal = MIRBuilder.buildCopy(S16, Register(Z80::SP));
    MIRBuilder.buildIntToPtr(Dst, SPVal);
    MI.eraseFromParent();
    return true;
  }

  case TargetOpcode::G_STACKRESTORE: {
    // G_STACKRESTORE %src:p0 → convert pointer to s16, write SP
    Register Src = MI.getOperand(0).getReg();
    LLT S16 = LLT::scalar(16);
    auto IntVal = MIRBuilder.buildPtrToInt(S16, Src);
    MIRBuilder.buildCopy(Register(Z80::SP), IntVal);
    MI.eraseFromParent();
    return true;
  }

  case TargetOpcode::G_VASTART: {
    // Store the address of the first vararg into the va_list pointer.
    MachineFunction &MF = MIRBuilder.getMF();
    Z80FunctionInfo *FuncInfo = MF.getInfo<Z80FunctionInfo>();
    int FI = FuncInfo->VarArgsStackIndex;
    LLT PtrTy = MRI.getType(MI.getOperand(0).getReg());
    auto FINAddr = MIRBuilder.buildFrameIndex(PtrTy, FI);
    assert(MI.hasOneMemOperand());
    MIRBuilder.buildStore(FINAddr, MI.getOperand(0).getReg(),
                          *MI.memoperands()[0]);
    MI.eraseFromParent();
    return true;
  }
  case TargetOpcode::G_LOAD: {
    // Extending G_LOAD (result type > memory type): decompose into
    // G_LOAD at memory width + G_ANYEXT.  Created by the pre-legalization
    // combiner folding G_ANYEXT(G_LOAD).
    Register Dst = MI.getOperand(0).getReg();
    Register Ptr = MI.getOperand(1).getReg();
    MachineMemOperand &MMO = **MI.memoperands_begin();
    LLT MemTy = MMO.getMemoryType();
    auto Load = MIRBuilder.buildLoad(MemTy, Ptr, MMO);
    MIRBuilder.buildAnyExt(Dst, Load);
    MI.eraseFromParent();
    return true;
  }
  case TargetOpcode::G_SEXTLOAD:
  case TargetOpcode::G_ZEXTLOAD: {
    // Decompose extending load into G_LOAD + G_SEXT/G_ZEXT.
    Register Dst = MI.getOperand(0).getReg();
    Register Ptr = MI.getOperand(1).getReg();
    MachineMemOperand &MMO = **MI.memoperands_begin();
    LLT MemTy = MMO.getMemoryType();
    auto Load = MIRBuilder.buildLoad(MemTy, Ptr, MMO);
    if (MI.getOpcode() == TargetOpcode::G_SEXTLOAD)
      MIRBuilder.buildSExt(Dst, Load);
    else
      MIRBuilder.buildZExt(Dst, Load);
    MI.eraseFromParent();
    return true;
  }

  case TargetOpcode::G_SMULH: {
    // smulh(a, b) = umulh(a, b) - (a >> 15) & b - (b >> 15) & a
    // This uses 1 umulh call vs the default lowering's 4 multiply calls.
    Register Dst = MI.getOperand(0).getReg();
    Register LHS = MI.getOperand(1).getReg();
    Register RHS = MI.getOperand(2).getReg();
    LLT Ty = MRI.getType(Dst);
    unsigned BitWidth = Ty.getSizeInBits();

    auto UMulH = MIRBuilder.buildInstr(TargetOpcode::G_UMULH, {Ty}, {LHS, RHS});
    auto ShiftAmt = MIRBuilder.buildConstant(Ty, BitWidth - 1);
    // sign_a = ashr(a, 15): all 0s or all 1s
    auto SignA = MIRBuilder.buildAShr(Ty, LHS, ShiftAmt);
    // sign_b = ashr(b, 15)
    auto SignB = MIRBuilder.buildAShr(Ty, RHS, ShiftAmt);
    // correction = (sign_a & b) + (sign_b & a)
    auto CorrA = MIRBuilder.buildAnd(Ty, SignA, RHS);
    auto CorrB = MIRBuilder.buildAnd(Ty, SignB, LHS);
    auto Corr = MIRBuilder.buildAdd(Ty, CorrA, CorrB);
    MIRBuilder.buildSub(Dst, UMulH, Corr);
    MI.eraseFromParent();
    return true;
  }

  case TargetOpcode::G_FADD:
  case TargetOpcode::G_FSUB:
  case TargetOpcode::G_FMUL:
  case TargetOpcode::G_FDIV: {
    // Custom float arithmetic legalization: select fast-math or IEEE libcall.
    // With nnan+ninf+nsz flags, calls __addsf3_fast etc. (skip NaN/Inf/zero
    // checks in the soft-float runtime).
    MachineFunction &MF = MIRBuilder.getMF();
    auto &Ctx = MF.getFunction().getContext();
    Type *F32Ty = Type::getFloatTy(Ctx);
    Register Dst = MI.getOperand(0).getReg();
    Register LHS = MI.getOperand(1).getReg();
    Register RHS = MI.getOperand(2).getReg();

    bool Fast = hasAllFastFlags(MI, MIRBuilder);
    const char *FuncName;
    switch (MI.getOpcode()) {
    case TargetOpcode::G_FADD:
      FuncName = Fast ? "__addsf3_fast" : "__addsf3";
      break;
    case TargetOpcode::G_FSUB:
      FuncName = Fast ? "__subsf3_fast" : "__subsf3";
      break;
    case TargetOpcode::G_FMUL:
      FuncName = Fast ? "__mulsf3_fast" : "__mulsf3";
      break;
    case TargetOpcode::G_FDIV:
      FuncName = Fast ? "__divsf3_fast" : "__divsf3";
      break;
    default:
      llvm_unreachable("unexpected opcode");
    }

    auto Status = Helper.createLibcall(FuncName, {Dst, F32Ty, 0},
                                       {{LHS, F32Ty, 0}, {RHS, F32Ty, 1}},
                                       CallingConv::C, LocObserver, &MI);
    if (Status != LegalizerHelper::Legalized)
      return false;
    MI.eraseFromParent();
    return true;
  }

  case TargetOpcode::G_FCONSTANT: {
    // Bitcast the floating-point constant to its integer bit representation.
    // The resulting i32 constant will be narrowed to two i16 values by the
    // existing G_CONSTANT legalization rules.
    const ConstantFP *CF = MI.getOperand(1).getFPImm();
    APInt IntVal = CF->getValueAPF().bitcastToAPInt();
    MIRBuilder.buildConstant(MI.getOperand(0).getReg(), IntVal);
    MI.eraseFromParent();
    return true;
  }

  case TargetOpcode::G_ICMP: {
    // Custom lowering for i32/i64/i128 G_ICMP: split into i16 halves and emit
    // G_Z80_ICMP32/64. The instruction selector handles these as chained
    // 8-bit SUB/SBC comparisons. Avoids the generic narrowScalar cascade
    // which generates extreme register pressure on Z80.
    //
    // i128: split into high/low i64 halves, compare high first; if equal,
    // compare low. This reuses the existing G_Z80_ICMP64 pseudo.
    Register Dst = MI.getOperand(0).getReg();
    auto Pred =
        static_cast<CmpInst::Predicate>(MI.getOperand(1).getPredicate());
    Register LHS = MI.getOperand(2).getReg();
    Register RHS = MI.getOperand(3).getReg();
    LLT S16 = LLT::scalar(16);
    LLT OpTy = MRI.getType(LHS);

    if (OpTy == LLT::scalar(128)) {
      // Split i128 into two i64 halves and compare using branch logic:
      // For EQ: (hi == hi) AND (lo == lo)
      // For NE: (hi != hi) OR (lo != lo)
      // For ordered: compare hi first; if hi equal, compare lo
      LLT S64 = LLT::scalar(64);
      LLT S8 = LLT::scalar(8);
      auto LHSHalves = MIRBuilder.buildUnmerge(S64, LHS);
      auto RHSHalves = MIRBuilder.buildUnmerge(S64, RHS);

      // Unmerge each i64 half into i16 parts for G_Z80_ICMP64
      auto LHSLoParts = MIRBuilder.buildUnmerge(S16, LHSHalves.getReg(0));
      auto LHSHiParts = MIRBuilder.buildUnmerge(S16, LHSHalves.getReg(1));
      auto RHSLoParts = MIRBuilder.buildUnmerge(S16, RHSHalves.getReg(0));
      auto RHSHiParts = MIRBuilder.buildUnmerge(S16, RHSHalves.getReg(1));

      if (Pred == CmpInst::ICMP_EQ || Pred == CmpInst::ICMP_NE) {
        // EQ: both halves must be equal
        // NE: either half differs
        Register HiCmp = MRI.createGenericVirtualRegister(S8);
        Register LoCmp = MRI.createGenericVirtualRegister(S8);
        MIRBuilder.buildInstr(Z80::G_Z80_ICMP64)
            .addDef(HiCmp)
            .addImm(static_cast<int64_t>(Pred))
            .addUse(LHSHiParts.getReg(0))
            .addUse(LHSHiParts.getReg(1))
            .addUse(LHSHiParts.getReg(2))
            .addUse(LHSHiParts.getReg(3))
            .addUse(RHSHiParts.getReg(0))
            .addUse(RHSHiParts.getReg(1))
            .addUse(RHSHiParts.getReg(2))
            .addUse(RHSHiParts.getReg(3));
        MIRBuilder.buildInstr(Z80::G_Z80_ICMP64)
            .addDef(LoCmp)
            .addImm(static_cast<int64_t>(Pred))
            .addUse(LHSLoParts.getReg(0))
            .addUse(LHSLoParts.getReg(1))
            .addUse(LHSLoParts.getReg(2))
            .addUse(LHSLoParts.getReg(3))
            .addUse(RHSLoParts.getReg(0))
            .addUse(RHSLoParts.getReg(1))
            .addUse(RHSLoParts.getReg(2))
            .addUse(RHSLoParts.getReg(3));
        if (Pred == CmpInst::ICMP_EQ)
          MIRBuilder.buildAnd(Dst, HiCmp, LoCmp);
        else
          MIRBuilder.buildOr(Dst, HiCmp, LoCmp);
      } else {
        // Ordered comparison: compare hi; if hi equal, use lo result
        Register HiCmp = MRI.createGenericVirtualRegister(S8);
        Register LoCmp = MRI.createGenericVirtualRegister(S8);
        Register HiEq = MRI.createGenericVirtualRegister(S8);

        // Compare using unsigned for lo half in signed comparisons
        CmpInst::Predicate LoPred =
            ICmpInst::isUnsigned(Pred)    ? Pred
            : (Pred == CmpInst::ICMP_SLT) ? CmpInst::ICMP_ULT
            : (Pred == CmpInst::ICMP_SLE) ? CmpInst::ICMP_ULE
            : (Pred == CmpInst::ICMP_SGT) ? CmpInst::ICMP_UGT
                                          : /* ICMP_SGE */ CmpInst::ICMP_UGE;

        MIRBuilder.buildInstr(Z80::G_Z80_ICMP64)
            .addDef(HiCmp)
            .addImm(static_cast<int64_t>(Pred))
            .addUse(LHSHiParts.getReg(0))
            .addUse(LHSHiParts.getReg(1))
            .addUse(LHSHiParts.getReg(2))
            .addUse(LHSHiParts.getReg(3))
            .addUse(RHSHiParts.getReg(0))
            .addUse(RHSHiParts.getReg(1))
            .addUse(RHSHiParts.getReg(2))
            .addUse(RHSHiParts.getReg(3));
        MIRBuilder.buildInstr(Z80::G_Z80_ICMP64)
            .addDef(HiEq)
            .addImm(static_cast<int64_t>(CmpInst::ICMP_EQ))
            .addUse(LHSHiParts.getReg(0))
            .addUse(LHSHiParts.getReg(1))
            .addUse(LHSHiParts.getReg(2))
            .addUse(LHSHiParts.getReg(3))
            .addUse(RHSHiParts.getReg(0))
            .addUse(RHSHiParts.getReg(1))
            .addUse(RHSHiParts.getReg(2))
            .addUse(RHSHiParts.getReg(3));
        MIRBuilder.buildInstr(Z80::G_Z80_ICMP64)
            .addDef(LoCmp)
            .addImm(static_cast<int64_t>(LoPred))
            .addUse(LHSLoParts.getReg(0))
            .addUse(LHSLoParts.getReg(1))
            .addUse(LHSLoParts.getReg(2))
            .addUse(LHSLoParts.getReg(3))
            .addUse(RHSLoParts.getReg(0))
            .addUse(RHSLoParts.getReg(1))
            .addUse(RHSLoParts.getReg(2))
            .addUse(RHSLoParts.getReg(3));
        // Result = hi_equal ? lo_cmp : hi_cmp
        MIRBuilder.buildSelect(Dst, HiEq, LoCmp, HiCmp);
      }

      MI.eraseFromParent();
      return true;
    }

    auto LHSParts = MIRBuilder.buildUnmerge(S16, LHS);
    auto RHSParts = MIRBuilder.buildUnmerge(S16, RHS);

    if (OpTy == LLT::scalar(64)) {
      MIRBuilder.buildInstr(Z80::G_Z80_ICMP64)
          .addDef(Dst)
          .addImm(static_cast<int64_t>(Pred))
          .addUse(LHSParts.getReg(0))
          .addUse(LHSParts.getReg(1))
          .addUse(LHSParts.getReg(2))
          .addUse(LHSParts.getReg(3))
          .addUse(RHSParts.getReg(0))
          .addUse(RHSParts.getReg(1))
          .addUse(RHSParts.getReg(2))
          .addUse(RHSParts.getReg(3));
    } else {
      MIRBuilder.buildInstr(Z80::G_Z80_ICMP32)
          .addDef(Dst)
          .addImm(static_cast<int64_t>(Pred))
          .addUse(LHSParts.getReg(0))
          .addUse(LHSParts.getReg(1))
          .addUse(RHSParts.getReg(0))
          .addUse(RHSParts.getReg(1));
    }

    MI.eraseFromParent();
    return true;
  }

  case TargetOpcode::G_FCMP: {
    // Custom FCMP lowering: call a comparison libcall that returns i16
    // (-1/0/+1), then use G_ICMP on the result. We can't use the default
    // FCMP libcall path because it returns i32, and our G_ICMP only
    // supports up to i16.
    //
    // GCC convention:
    //   __cmpsf2: returns -1 (a<b), 0 (a==b), +1 (a>b or NaN)
    //   __gtsf2/__gesf2: returns -1 (a<b or NaN), 0 (a==b), +1 (a>b)
    //   __unordsf2: returns nonzero if either operand is NaN, 0 otherwise
    //
    // Ordered predicates return false for NaN - handled naturally by the
    // libcall return values. Unordered predicates return true for NaN -
    // need an additional __unordsf2 call. FCMP_ONE also needs __unordsf2
    // because __cmpsf2 returns +1 for NaN which falsely satisfies != 0.
    MachineFunction &MF = MIRBuilder.getMF();
    auto Pred =
        static_cast<CmpInst::Predicate>(MI.getOperand(1).getPredicate());
    Register Dst = MI.getOperand(0).getReg();
    Register LHS = MI.getOperand(2).getReg();
    Register RHS = MI.getOperand(3).getReg();
    auto &Ctx = MF.getFunction().getContext();
    Type *F32Ty = Type::getFloatTy(Ctx);
    Type *I16Ty = Type::getInt16Ty(Ctx);
    LLT S16 = LLT::scalar(16);
    LLT S1 = LLT::scalar(1);

    bool Fast = hasAllFastFlags(MI, MIRBuilder);

    // FCMP_ORD/UNO: only need __unordsf2.
    if (Pred == CmpInst::FCMP_ORD || Pred == CmpInst::FCMP_UNO) {
      if (Fast) {
        // nnan+ninf+nsz: ORD is always true, UNO is always false.
        MIRBuilder.buildConstant(Dst, Pred == CmpInst::FCMP_ORD ? 1 : 0);
        MI.eraseFromParent();
        return true;
      }
      Register UnordResult = MRI.createGenericVirtualRegister(S16);
      auto Status = Helper.createLibcall("__unordsf2", {UnordResult, I16Ty, 0},
                                         {{LHS, F32Ty, 0}, {RHS, F32Ty, 1}},
                                         CallingConv::C, LocObserver, &MI);
      if (Status != LegalizerHelper::Legalized)
        return false;
      auto Zero = MIRBuilder.buildConstant(S16, 0);
      CmpInst::Predicate ICmpP =
          (Pred == CmpInst::FCMP_ORD) ? CmpInst::ICMP_EQ : CmpInst::ICMP_NE;
      MIRBuilder.buildICmp(ICmpP, Dst, UnordResult, Zero);
      MI.eraseFromParent();
      return true;
    }

    // Determine if we need an __unordsf2 call for NaN handling.
    // UNE is already correct without it (__cmpsf2 returns +1 for NaN).
    // With nnan, NaN never occurs so extra NaN check is never needed.
    bool IsUnordered = CmpInst::isUnordered(Pred);
    bool IsONE = (Pred == CmpInst::FCMP_ONE);
    bool NeedsNaNCheck =
        !Fast && ((IsUnordered && Pred != CmpInst::FCMP_UNE) || IsONE);

    // Map unordered predicates to their ordered equivalents.
    CmpInst::Predicate OrderedPred =
        IsUnordered ? CmpInst::getOrderedPredicate(Pred) : Pred;

    const char *LibcallName;
    CmpInst::Predicate ICmpPred;
    switch (OrderedPred) {
    case CmpInst::FCMP_OEQ:
      LibcallName = Fast ? "__cmpsf2_fast" : "__cmpsf2";
      ICmpPred = CmpInst::ICMP_EQ;
      break;
    case CmpInst::FCMP_OLT:
      LibcallName = Fast ? "__cmpsf2_fast" : "__cmpsf2";
      ICmpPred = CmpInst::ICMP_SLT;
      break;
    case CmpInst::FCMP_OLE:
      LibcallName = Fast ? "__cmpsf2_fast" : "__cmpsf2";
      ICmpPred = CmpInst::ICMP_SLE;
      break;
    case CmpInst::FCMP_OGT:
      LibcallName = Fast ? "__cmpsf2_fast" : "__gtsf2";
      ICmpPred = CmpInst::ICMP_SGT;
      break;
    case CmpInst::FCMP_OGE:
      LibcallName = Fast ? "__cmpsf2_fast" : "__gesf2";
      ICmpPred = CmpInst::ICMP_SGE;
      break;
    case CmpInst::FCMP_ONE:
      LibcallName = Fast ? "__cmpsf2_fast" : "__cmpsf2";
      ICmpPred = CmpInst::ICMP_NE;
      break;
    default:
      return false;
    }

    Register CmpResult = MRI.createGenericVirtualRegister(S16);
    auto Status = Helper.createLibcall(LibcallName, {CmpResult, I16Ty, 0},
                                       {{LHS, F32Ty, 0}, {RHS, F32Ty, 1}},
                                       CallingConv::C, LocObserver, &MI);
    if (Status != LegalizerHelper::Legalized)
      return false;

    if (!NeedsNaNCheck) {
      // Ordered predicates (except ONE) and UNE: no extra NaN handling.
      auto Zero = MIRBuilder.buildConstant(S16, 0);
      MIRBuilder.buildICmp(ICmpPred, Dst, CmpResult, Zero);
    } else {
      // Need __unordsf2 call for NaN handling.
      Register UnordResult = MRI.createGenericVirtualRegister(S16);
      auto UStatus = Helper.createLibcall("__unordsf2", {UnordResult, I16Ty, 0},
                                          {{LHS, F32Ty, 0}, {RHS, F32Ty, 1}},
                                          CallingConv::C, LocObserver, &MI);
      if (UStatus != LegalizerHelper::Legalized)
        return false;

      auto Zero = MIRBuilder.buildConstant(S16, 0);
      Register CmpBool = MRI.createGenericVirtualRegister(S1);
      MIRBuilder.buildICmp(ICmpPred, CmpBool, CmpResult, Zero);

      Register NaNBool = MRI.createGenericVirtualRegister(S1);
      if (IsONE) {
        // ONE: (cmp != 0) AND NOT unordered
        MIRBuilder.buildICmp(CmpInst::ICMP_EQ, NaNBool, UnordResult, Zero);
        MIRBuilder.buildAnd(Dst, CmpBool, NaNBool);
      } else {
        // Unordered: ordered_result OR unordered
        MIRBuilder.buildICmp(CmpInst::ICMP_NE, NaNBool, UnordResult, Zero);
        MIRBuilder.buildOr(Dst, CmpBool, NaNBool);
      }
    }

    MI.eraseFromParent();
    return true;
  }

  case TargetOpcode::G_MEMSET: {
    // C memset takes (void*, int, size_t). On Z80, int = i16.
    // G_MEMSET has i8 val operand which must be promoted to i16
    // so the calling convention assigns it to DE (2nd i16 reg param)
    // instead of treating it as an i8 arg.
    Register ValReg = MI.getOperand(1).getReg();
    LLT ValTy = MRI.getType(ValReg);

    if (ValTy.getSizeInBits() < 16) {
      MIRBuilder.setInsertPt(*MI.getParent(), MI.getIterator());
      auto ZExt = MIRBuilder.buildZExt(LLT::scalar(16), ValReg);
      MI.getOperand(1).setReg(ZExt.getReg(0));
    }

    auto Result = Helper.createMemLibcall(MRI, MI, LocObserver);
    if (Result != LegalizerHelper::Legalized)
      return false;
    MI.eraseFromParent();
    return true;
  }
  case TargetOpcode::G_SEXT_INREG: {
    // Width==8: keep as legal for instruction selector (SEXT_GR8_GR16 pseudo).
    // Other widths (e.g., 1 for bool→signed): lower to SHL + ASHR.
    int64_t Width = MI.getOperand(2).getImm();
    if (Width == 8)
      return true;

    Register DstReg = MI.getOperand(0).getReg();
    Register SrcReg = MI.getOperand(1).getReg();
    LLT Ty = MRI.getType(DstReg);
    unsigned ShiftAmt = Ty.getSizeInBits() - Width;
    auto ShiftC = MIRBuilder.buildConstant(LLT::scalar(8), ShiftAmt);
    auto Shl = MIRBuilder.buildShl(Ty, SrcReg, ShiftC);
    MIRBuilder.buildAShr(DstReg, Shl, ShiftC);
    MI.eraseFromParent();
    return true;
  }
  case TargetOpcode::G_BRJT: {
    // Custom lower G_BRJT to: SHL index + PTR_ADD + LOAD + G_BRINDIRECT
    // Following RISCV's legalizeBRJT pattern.
    auto &MF = *MI.getParent()->getParent();
    const MachineJumpTableInfo *MJTI = MF.getJumpTableInfo();
    unsigned EntrySize = MJTI->getEntrySize(MF.getDataLayout());

    Register PtrReg = MI.getOperand(0).getReg();
    LLT PtrTy = MRI.getType(PtrReg);
    Register IndexReg = MI.getOperand(2).getReg();
    LLT IndexTy = MRI.getType(IndexReg);

    assert(isPowerOf2_32(EntrySize) &&
           "Jump table entry size must be power of 2");

    // Scale index by entry size (2 bytes for Z80 16-bit pointers)
    if (unsigned Shift = Log2_32(EntrySize)) {
      auto ShiftAmt = MIRBuilder.buildConstant(LLT::scalar(8), Shift);
      IndexReg = MIRBuilder.buildShl(IndexTy, IndexReg, ShiftAmt).getReg(0);
    }

    // Compute address: table_base + scaled_index
    auto Addr = MIRBuilder.buildPtrAdd(PtrTy, PtrReg, IndexReg);

    // Load target address from jump table
    MachineMemOperand *MMO = MF.getMachineMemOperand(
        MachinePointerInfo::getJumpTable(MF), MachineMemOperand::MOLoad,
        EntrySize, Align(MJTI->getEntryAlignment(MF.getDataLayout())));
    auto Target = MIRBuilder.buildLoad(PtrTy, Addr, *MMO);

    // Indirect branch to loaded target
    MIRBuilder.buildBrIndirect(Target.getReg(0));

    MI.eraseFromParent();
    return true;
  }
  case TargetOpcode::G_SHL:
  case TargetOpcode::G_LSHR:
  case TargetOpcode::G_ASHR: {
    // Non-power-of-2 shift types (e.g. s17 from SCEV closed-form sum).
    // Widen to next power of 2, then let standard legalization handle it.
    // We do this in custom rather than widenScalarToNextPow2 because the
    // generic widenScalar path creates G_ZEXT(s17→s32) intermediates that
    // cascade into more non-pow2 types. By handling it here, we directly
    // emit pow2-width operations that the standard narrowScalar can process.
    Register DstReg = MI.getOperand(0).getReg();
    Register SrcReg = MI.getOperand(1).getReg();
    Register AmtReg = MI.getOperand(2).getReg();
    LLT SrcTy = MRI.getType(SrcReg);
    unsigned Size = SrcTy.getSizeInBits();
    unsigned WideSize = llvm::PowerOf2Ceil(Size);
    LLT WideTy = LLT::scalar(WideSize);

    unsigned ExtOpc;
    switch (MI.getOpcode()) {
    case TargetOpcode::G_LSHR:
      ExtOpc = TargetOpcode::G_ZEXT;
      break;
    case TargetOpcode::G_ASHR:
      ExtOpc = TargetOpcode::G_SEXT;
      break;
    default:
      ExtOpc = TargetOpcode::G_ANYEXT;
      break;
    }
    auto WideSrc = MIRBuilder.buildInstr(ExtOpc, {WideTy}, {SrcReg});
    auto WideShift =
        MIRBuilder.buildInstr(MI.getOpcode(), {WideTy}, {WideSrc, AmtReg});
    MIRBuilder.buildTrunc(DstReg, WideShift);
    MI.eraseFromParent();
    return true;
  }
  default:
    return false;
  }
}
