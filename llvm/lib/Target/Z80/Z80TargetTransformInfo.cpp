//===- Z80TargetTransformInfo.cpp - Z80 specific TTI ----------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the Z80-specific TargetTransformInfo cost hooks.  The
// guiding facts about the target:
//   * 3 allocatable 16-bit register pairs (BC/DE/HL) + IX/IY (prefixed, costly)
//     -> extreme register pressure; spills are expensive (3-4 B each).
//   * 8-bit accumulator-centric ALU; 16-bit ALU is limited to ADD HL,rr / INC /
//     DEC; there is no hardware multiply (mul/div/rem are libcalls).
//   * No branch predictor; no SIMD / vectorization.
//
// Costs are reported CostKind-aware where the byte cost (TCK_CodeSize /
// TCK_SizeAndLatency) and the throughput/latency cost (TCK_RecipThroughput /
// TCK_Latency) genuinely differ on Z80.
//
//===----------------------------------------------------------------------===//

#include "Z80TargetTransformInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;

#define DEBUG_TYPE "z80tti"

// ravn/llvm-z80#177: the shift-width and wider-cast cost refinements (Steps 2/3)
// are *accurate* but, on every workload measured (AES corpus, cpnos, BIOS, the
// compiler-comparison-corpus, a CRC/MAC/bitfield microbench), they change NO
// codegen -- the passes that consult them (LoopUnroll/SimplifyCFG/SelectOptimize/
// SpeculativeExecution/IROutliner/ConstantHoisting/CGP/TruncInstCombine) have no
// cheaper Z80 alternative to select.  Kept behind this hidden, default-off flag
// so they ship as documented/tested model-accuracy without touching default
// codegen until a workload proves them out.  The isLegalAddImmediate fix (Step 1)
// is a real codegen win and is NOT gated.
static cl::opt<bool> EnableExperimentalCosts(
    "z80-experimental-tti-costs", cl::init(false), cl::Hidden,
    cl::desc("Z80: enable the accurate-but-currently-inert shift-width and "
             "wider-integer cast costs (ravn/llvm-z80#177 Steps 2/3).  Default "
             "off: proven codegen-neutral on all measured workloads."));

// All div, rem, and divrem ops are libcalls on Z80, so any possible combination exists.
bool Z80TTIImpl::hasDivRemOp(Type *DataType, bool IsSigned) const {
  return true;
}

// In Loop Strength Reduction (LSR), prioritize minimizing register count over
// instruction count, because Z80 has extreme register pressure (only 3 pairs).
bool Z80TTIImpl::isLSRCostLess(const TargetTransformInfo::LSRCost &C1,
                               const TargetTransformInfo::LSRCost &C2) const {
  return std::tie(C1.NumRegs, C1.Insns, C1.AddRecCost, C1.NumIVMuls,
                  C1.NumBaseAdds, C1.ScaleCost, C1.ImmCost, C1.SetupCost) <
         std::tie(C2.NumRegs, C2.Insns, C2.AddRecCost, C2.NumIVMuls,
                  C2.NumBaseAdds, C2.ScaleCost, C2.ImmCost, C2.SetupCost);
}

// Z80 has no branch prediction hardware; branches execute in fixed T-states.
BranchProbability Z80TTIImpl::getPredictableBranchThreshold() const {
  return BranchProbability(0, 1);
}

// Z80 uses a single flat 16-bit address space, so all address-space casts are valid.
bool Z80TTIImpl::isValidAddrSpaceCast(unsigned FromAS, unsigned ToAS) const {
  return true;
}

// Report 3 allocatable 16-bit GP register pairs (BC, DE, HL). Reporting 7 8-bit
// registers would overstate pair availability and cause loop passes to create
// excess induction variables that spill.
unsigned Z80TTIImpl::getNumberOfRegisters(unsigned ClassID) const {
  return 3;
}

// Report the native 8-bit scalar register width of the Z80 CPU.
TypeSize
Z80TTIImpl::getRegisterBitWidth(TargetTransformInfo::RegisterKind K) const {
  return TypeSize::getFixed(8);
}

// Guard against register exhaustion from inlining large functions.
// Call overhead (CALL 3 B, RET 1 B) is only worth eliminating for small functions
// (<= 10 instructions), functions with explicit inline hints, or single-caller internal functions.
bool Z80TTIImpl::areInlineCompatible(const Function *Caller,
                                     const Function *Callee) const {
  if (Callee->hasFnAttribute(Attribute::InlineHint))
    return true;
  if (Callee->getInstructionCount() <= 10)
    return true;
  if (Callee->hasInternalLinkage() && Callee->hasOneUse())
    return true;
  return false;
}

// Z80 has no SIMD/vector hardware; disable vector-style addressing heuristics in LSR.
bool Z80TTIImpl::prefersVectorizedAddressing() const { return false; }

// "Legal add immediate" means the target can add this constant to a register
// WITHOUT first materializing it into another register.  Z80 has no
// `ADD HL,nn` / `ADD rr,nn`: a 16-bit constant add costs `LD rr,nn` (3 B) +
// `ADD HL,rr` (1 B) = 4 B, the value materialized first.  The only
// no-materialize option is a chain of `INC/DEC rr` (1 B each), strictly
// cheaper than materialize-and-add only for |Imm| <= 3 (3 B vs 4 B; |Imm|==4
// breaks even).  The TargetLoweringBase default returns true for EVERY
// immediate -- inaccurate for Z80.
//
// Effect (measured, not assumed): LSR consults this when scoring formulas.
// With the default, for a loop touching several arrays at fixed offsets from
// one base (the aes_done key-zeroing shape; see the lit test), LSR keeps a
// single base IV + folded member offsets -- a shape that does not fit Z80's 3
// register pairs, so the backend spills a pointer pair into the loop.
// Reporting the truth changes LSR's formula/IV choice so the pointers stay in
// registers and the in-loop spill disappears.  NB: the corrected model does
// NOT eliminate large-immediate adds (the strength-reduced code still has
// `ld de,nn; add hl,de` steps); the win is reduced register pressure, net
// fewer instructions.  Measured on the AES corpus: -8..-124 B and faster on
// every LSR-active config; production configs (-disable-lsr) byte-identical,
// so this cannot regress the production targets.  (ravn/llvm-z80#177.)
bool Z80TTIImpl::isLegalAddImmediate(int64_t Imm) const {
  return Imm >= -3 && Imm <= 3;
}

// DO NOT add an i16=2 / i32=4 integer-arithmetic width charge here.  That is
// ravn/llvm-z80#184, closed WONT-FIX.  Full mechanism + evidence:
// `tasks/issue184-wontfix-mechanism-2026-05-30.md`.  In short:
//   * It is correctness-safe (the original "infinite loop" was peephole bugs
//     #148/#185, since fixed) but it is an *inaccurate* cost.  On Z80 a 16-bit
//     add is ~1 byte (`inc bc` / `add hl,rr`) -- the SAME as an 8-bit add -- so
//     the accurate cost is ~1, i.e. the BasicTTIImpl baseline.
//   * Its only benefit (AES, where IVs are genuinely 8-bit) comes from the
//     inaccuracy: it makes IndVarSimplify stop widening 8-bit IVs.  The same
//     overcharge REGRESSES production: e.g. cpnos `_netboot_mpm` has an i8
//     counter that SHOULD widen to a pair (`inc bc`); i16=2 suppresses that and
//     the 8-bit counter is shuttled through A + a BSS slot (78->84 insns,
//     cpnos PROM1 2022->2033 B, into the 2 KB cap).
//   * Not fixable by a better number: the real cost is the post-regalloc,
//     register-pressure-dominated allocation outcome, but the decision is made
//     pre-regalloc and the cost API is a per-(opcode,type) scalar with no slot
//     for "in a pair vs spilled."  A proper fix is an allocation-aware Z80
//     narrowing pass (a smarter Z80NarrowIV), not a cost edit.
InstructionCost Z80TTIImpl::getArithmeticInstrCost(
    unsigned Opcode, Type *Ty, TargetTransformInfo::TargetCostKind CostKind,
    TargetTransformInfo::OperandValueInfo Op1Info,
    TargetTransformInfo::OperandValueInfo Op2Info,
    ArrayRef<const Value *> Args, const Instruction *CxtI) const {
  // Z80 has no hardware multiplier; mul is a libcall.  Default
  // returns TCC_Expensive for SDiv/SRem/UDiv/URem/FDiv/FRem but
  // not Mul.  Add Mul to the libcall set.
  if (Opcode == Instruction::Mul)
    return TargetTransformInfo::TCC_Expensive;

  // Z80 has no barrel shifter.  Each bit of a shift is a separate single-bit
  // shift/rotate (SLA/SRL/SRA r = 2 B/8 T; left-shift can use ADD HL,HL for
  // i16), and a *variable* shift count compiles to a runtime loop.  The
  // BasicTTIImpl default reports legal shifts as cost 1 -- far too cheap.
  // Cost scales with BOTH the shift amount AND the width in bytes: an N-byte
  // shift moves one bit across all N limbs (RR/SLA + carry) per position, so a
  // 32-bit shift-by-1 is ~4 ops, not 1.  Undercharging wide shifts as 1 made
  // LoopUnroll fully unroll wide-int shift loops (e.g. a CRC inner loop blew up
  // ~3.4x at -O2).  A variable count compiles to a runtime loop -> expensive.
  if (EnableExperimentalCosts &&
      (Opcode == Instruction::Shl || Opcode == Instruction::LShr ||
       Opcode == Instruction::AShr)) {
    unsigned Bytes = std::max(1u, (Ty->getScalarSizeInBits() + 7) / 8);
    const ConstantInt *AmtC = nullptr;
    if (Args.size() == 2)
      AmtC = dyn_cast_or_null<ConstantInt>(Args[1]);
    if (!AmtC && CxtI && CxtI->getNumOperands() == 2)
      AmtC = dyn_cast_or_null<ConstantInt>(CxtI->getOperand(1));
    if (!AmtC)
      return TargetTransformInfo::TCC_Expensive * Bytes; // variable -> loop
    unsigned Amt = AmtC->getValue().getZExtValue();
    if (Amt == 0)
      return 0;
    // ~Bytes single-bit ops per shifted position; cap the per-position amount
    // (the ShiftRotateChain pass open-codes wide constant shifts).
    return std::min(Amt, 8u) * Bytes;
  }

  return BaseT::getArithmeticInstrCost(Opcode, Ty, CostKind, Op1Info, Op2Info,
                                       Args, CxtI);
}

InstructionCost
Z80TTIImpl::getCastInstrCost(unsigned Opcode, Type *Dst, Type *Src,
                             TargetTransformInfo::CastContextHint CCH,
                             TargetTransformInfo::TargetCostKind CostKind,
                             const Instruction *I) const {
  if (Dst->isIntegerTy() && Src->isIntegerTy()) {
    unsigned DstBits = Dst->getIntegerBitWidth();
    unsigned SrcBits = Src->getIntegerBitWidth();
    if (EnableExperimentalCosts) {
      // Step 3 (#177, default-off -- proven codegen-neutral): the accurate,
      // width-general, monotonic cast model.
      // Truncation is always free on Z80 -- the narrow value is the low bytes
      // of the wide one; the high bytes are dropped.  (Base charges 1 for e.g.
      // i64->i32; correct that to 0 for all widths.)
      if (Opcode == Instruction::Trunc && DstBits < SrcBits)
        return 0;
      // Zero-extension clears the new high bytes; i8->i16 ~ free (LD H,0),
      // wider zexts fall through to the base (~1).
      if (Opcode == Instruction::ZExt && SrcBits == 8 && DstBits == 16)
        return 0;
      // Sign-extension computes the sign (LD A,lo; RLCA; SBC A,A ~ 2 ops) then
      // splats it into each new high byte; scales with added bytes and is
      // monotonic in width (base undercharges wider sexts, reporting
      // sext i8->i32 cheaper than sext i8->i16, which is backwards).
      if (Opcode == Instruction::SExt && DstBits > SrcBits) {
        unsigned ExtraBytes = (DstBits - SrcBits + 7) / 8;
        return 1 + ExtraBytes; // i8->i16:2 i16->i32:3 i8->i32:4 i32->i64:5
      }
    } else {
      // Default: the originally-shipped i8<->i16 cases only (pre-#177-Step-3).
      if (Opcode == Instruction::Trunc && SrcBits == 16 && DstBits == 8)
        return 0;
      if (Opcode == Instruction::ZExt && SrcBits == 8 && DstBits == 16)
        return 0;
      if (Opcode == Instruction::SExt && SrcBits == 8 && DstBits == 16)
        return 2;
    }
  }
  return BaseT::getCastInstrCost(Opcode, Dst, Src, CCH, CostKind, I);
}
