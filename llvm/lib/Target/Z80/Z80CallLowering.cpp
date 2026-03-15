//===-- Z80CallLowering.cpp - Z80 Call lowering -----------------*- C++ -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the lowering of LLVM calls to machine code calls for
// GlobalISel. Z80CallLoweringCommon provides the shared implementation;
// Z80CallLowering supplies Z80-specific register mappings.
//
//===----------------------------------------------------------------------===//

#include "Z80CallLowering.h"

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80FrameLowering.h"
#include "Z80MachineFunctionInfo.h"
#include "Z80RegisterInfo.h"
#include "Z80Subtarget.h"

#include "llvm/CodeGen/Analysis.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/FunctionLoweringInfo.h"
#include "llvm/CodeGen/GlobalISel/MachineIRBuilder.h"
#include "llvm/CodeGen/GlobalISel/Utils.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/CodeGen/TargetCallingConv.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/CodeGen/TargetOpcodes.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/Target/TargetMachine.h"
#include <memory>

using namespace llvm;

#define DEBUG_TYPE "z80-call-lowering"

// SDCC __sdcccall(1) argument classification.
// Determines whether an argument goes in a register or on the stack.
namespace {

/// Get the return type size in bits, handling struct types correctly.
/// getPrimitiveSizeInBits() returns 0 for structs; we need the actual size
/// to match SDCC's cleanup decision which uses the declared return type size.
static unsigned getReturnTypeSizeInBits(Type *RetTy, const DataLayout &DL) {
  if (RetTy->isVoidTy())
    return 0;
  unsigned Bits = RetTy->getPrimitiveSizeInBits();
  if (Bits == 0 && (RetTy->isStructTy() || RetTy->isArrayTy()))
    Bits = DL.getTypeAllocSizeInBits(RetTy);
  return Bits;
}

/// Determine if callee cleans up stack arguments.
/// sdcccall(0): always caller cleanup (returns false).
/// sdcccall(1):
///   Z80: callee cleanup when non-variadic AND (return ≤16 bits OR
///        (return float AND first arg float)).
///   SM83: callee cleanup when non-variadic (always).
static bool isCalleeCleanup(bool IsVarArg, Type *RetTy, Type *FirstArgTy,
                            bool IsSM83, CallingConv::ID CC,
                            const DataLayout &DL) {
  if (CC == CallingConv::Z80_SDCCCall0)
    return false;
  if (IsVarArg)
    return false;
  if (IsSM83)
    return true;
  // Z80: return ≤16 bits → callee cleanup
  unsigned RetBits = getReturnTypeSizeInBits(RetTy, DL);
  if (RetBits <= 16)
    return true;
  // Z80 float exception: return float AND first arg float → callee cleanup
  if (RetTy->isFloatTy() && FirstArgTy && FirstArgTy->isFloatTy())
    return true;
  return false;
}
enum FirstArgKind { FIRST_NONE, FIRST_I8, FIRST_I16, FIRST_I32 };

struct ArgAssignment {
  bool InReg = false;
  Register PhysReg;  // Primary register
  Register PhysReg2; // Secondary register for i32
  FirstArgKind NewFirstKind = FIRST_NONE;
};

/// Classify a single argument based on current register state.
/// Uses CallingConvRegs to look up the correct physical registers.
/// sdcccall(0): all args go on stack (returns empty result).
ArgAssignment classifyArg(const CallingConvRegs &Regs, unsigned &RegParamCount,
                          FirstArgKind &FirstKind, unsigned BitWidth,
                          bool IsVarArg, CallingConv::ID CC = CallingConv::C) {
  ArgAssignment Result;

  if (CC == CallingConv::Z80_SDCCCall0 || IsVarArg)
    return Result;

  if (RegParamCount == 0) {
    if (BitWidth <= 8) {
      Result = {true, Z80::A, Register(), FIRST_I8};
    } else if (BitWidth <= 16) {
      Result = {true, Regs.First_I16, Register(), FIRST_I16};
    } else if (BitWidth <= 32) {
      Result = {true, Regs.First_I32_Hi, Regs.First_I32_Lo, FIRST_I32};
    }
    if (Result.InReg)
      FirstKind = Result.NewFirstKind;
    // Always consume parameter position (SDCC counts arg positions, not
    // register assignments). If this arg fails to get a register, subsequent
    // args must also go to stack.
    RegParamCount++;
  } else if (RegParamCount == 1) {
    if (FirstKind == FIRST_I8) {
      if (BitWidth <= 8)
        Result = {true, Regs.Second_AfterI8_I8, Register(), FIRST_NONE};
      else if (BitWidth <= 16)
        Result = {true, Regs.Second_AfterI8_I16, Register(), FIRST_NONE};
    } else if (FirstKind == FIRST_I16) {
      if (BitWidth <= 8 && Regs.Second_AfterI16_I8.isValid())
        Result = {true, Regs.Second_AfterI16_I8, Register(), FIRST_NONE};
      else if (BitWidth > 8 && BitWidth <= 16)
        Result = {true, Regs.Second_AfterI16_I16, Register(), FIRST_NONE};
    }
    RegParamCount++;
  }

  return Result;
}
} // anonymous namespace

//===----------------------------------------------------------------------===//
// Z80CallLowering — Z80-specific register config
//===----------------------------------------------------------------------===//

Z80CallLowering::Z80CallLowering(const TargetLowering *TL)
    : Z80CallLoweringCommon(TL,
                            // sdcccall(1) registers
                            CallingConvRegs{
                                /*First_I16=*/Z80::HL,
                                /*First_I32_Hi=*/Z80::HL,
                                /*First_I32_Lo=*/Z80::DE,
                                /*Second_AfterI8_I8=*/Z80::L,
                                /*Second_AfterI8_I16=*/Z80::DE,
                                /*Second_AfterI16_I8=*/Register(),
                                /*Second_AfterI16_I16=*/Z80::DE,
                                /*Ret_I8=*/Z80::A,
                                /*Ret_I16=*/Z80::DE,
                                /*Ret_I32_Hi=*/Z80::HL,
                                /*Ret_I32_Lo=*/Z80::DE,
                                /*IndirectCallReg=*/Z80::IY,
                                /*IndirectCallOpc=*/Z80::CALL_IY,
                            },
                            // sdcccall(0) registers
                            CallingConvRegs{
                                /*First_I16=*/Register(),
                                /*First_I32_Hi=*/Register(),
                                /*First_I32_Lo=*/Register(),
                                /*Second_AfterI8_I8=*/Register(),
                                /*Second_AfterI8_I16=*/Register(),
                                /*Second_AfterI16_I8=*/Register(),
                                /*Second_AfterI16_I16=*/Register(),
                                /*Ret_I8=*/Z80::L,
                                /*Ret_I16=*/Z80::HL,
                                /*Ret_I32_Hi=*/Z80::DE,
                                /*Ret_I32_Lo=*/Z80::HL,
                                /*IndirectCallReg=*/Z80::IY,
                                /*IndirectCallOpc=*/Z80::CALL_IY,
                            }) {}

//===----------------------------------------------------------------------===//
// Z80CallLoweringCommon — shared implementation
//===----------------------------------------------------------------------===//

bool Z80CallLoweringCommon::lowerReturn(MachineIRBuilder &MIRBuilder,
                                        const Value *Val,
                                        ArrayRef<Register> VRegs,
                                        FunctionLoweringInfo &FLI) const {
  MachineFunction &MF = MIRBuilder.getMF();

  // Check if this is an interrupt handler (uses RETI instead of RET).
  bool IsInterrupt = MF.getFunction().hasFnAttribute("interrupt");

  // Determine if this function uses callee-cleanup (RET_CLEANUP vs RET).
  Z80FunctionInfo *FuncInfo = MF.getInfo<Z80FunctionInfo>();
  unsigned CleanupBytes = FuncInfo->StackParamBytes;

  // Helper to emit either RET, RET_CLEANUP, or RETI with optional implicit
  // uses.
  auto emitRet =
      [&](ArrayRef<std::pair<Register, RegState>> ImplicitUses = {}) {
        MachineInstrBuilder RetMI;
        if (IsInterrupt) {
          const auto &STI = MF.getSubtarget<Z80Subtarget>();
          unsigned RetiOpc = STI.hasSM83() ? Z80::SM83_RETI : Z80::RETI;
          RetMI = MIRBuilder.buildInstr(RetiOpc);
        } else if (CleanupBytes > 0)
          RetMI = MIRBuilder.buildInstr(Z80::RET_CLEANUP).addImm(CleanupBytes);
        else
          RetMI = MIRBuilder.buildInstr(Z80::RET);
        for (auto [Reg, Flags] : ImplicitUses)
          RetMI.addUse(Reg, Flags);
      };

  // If there's no return value, just emit RET
  if (VRegs.empty() || Val == nullptr) {
    emitRet();
    return true;
  }

  // Handle sret demotion: store return value through sret pointer
  if (!FLI.CanLowerReturn) {
    insertSRetStores(MIRBuilder, Val->getType(), VRegs, FLI.DemoteRegister);
    emitRet();
    return true;
  }

  // Select register config based on calling convention.
  CallingConv::ID CC = MF.getFunction().getCallingConv();
  const CallingConvRegs &Regs = getRegsForCC(CC);

  // Return value conventions: i8->Ret_I8, i16->Ret_I16,
  // i32->Ret_I32_Hi:Ret_I32_Lo
  Type *RetTy = Val->getType();
  MachineRegisterInfo &MRI = MF.getRegInfo();

  // Handle aggregate types (structs, arrays) by packing fields into return
  // registers via a temporary stack slot. Each field is stored at its
  // DataLayout offset, then the slot is reloaded as scalar 16-bit words
  // matching the return register convention.
  if (RetTy->isAggregateType()) {
    const DataLayout &DL = MF.getDataLayout();
    unsigned AllocSize = DL.getTypeAllocSize(RetTy);

    if (AllocSize > 4)
      return false; // Should have been caught by canLowerReturn

    // Decompose struct into field types and byte offsets.
    SmallVector<EVT, 4> SplitVTs;
    SmallVector<uint64_t, 4> Offsets;
    ComputeValueVTs(*getTLI(), DL, RetTy, SplitVTs, nullptr, &Offsets, 0);
    assert(VRegs.size() == SplitVTs.size());

    // Pack aggregate fields directly into return registers using
    // field-per-slot assignment. Each field maps to a 16-bit register
    // slot; i8 fields are anyext'd, adjacent i8 pairs are merged.
    // This avoids unnecessary stack round-trips.
    //
    // Mapping (Rust ABI, symmetric with caller's unpack in lowerCall):
    //   ≤ 1B: Ret_I8
    //   ≤ 2B: fields merged/copied → Ret_I16
    //   3-4B: fields assigned to Ret_I32_Lo / Ret_I32_Hi
    //
    // Helper: pack a sequence of fields into a single i16 register.
    // Returns the packed register, or an invalid Register on failure.
    auto packFieldsIntoWord = [&](ArrayRef<unsigned> FieldIndices) -> Register {
      if (FieldIndices.size() == 1) {
        unsigned Idx = FieldIndices[0];
        unsigned Bits = SplitVTs[Idx].getSizeInBits();
        if (Bits == 16)
          return VRegs[Idx];
        if (Bits == 8) {
          Register Ext = MRI.createGenericVirtualRegister(LLT::scalar(16));
          MIRBuilder.buildAnyExt(Ext, VRegs[Idx]);
          return Ext;
        }
        return Register();
      }
      if (FieldIndices.size() == 2) {
        unsigned Idx0 = FieldIndices[0], Idx1 = FieldIndices[1];
        if (SplitVTs[Idx0].getSizeInBits() == 8 &&
            SplitVTs[Idx1].getSizeInBits() == 8) {
          Register Merged = MRI.createGenericVirtualRegister(LLT::scalar(16));
          MIRBuilder.buildMergeLikeInstr(Merged, {VRegs[Idx0], VRegs[Idx1]});
          return Merged;
        }
        return Register();
      }
      return Register();
    };

    // Validate: all fields must be i8 or i16.
    bool AllFieldsValid = true;
    for (unsigned I = 0; I < SplitVTs.size(); ++I) {
      unsigned Bits = SplitVTs[I].getSizeInBits();
      if (Bits != 8 && Bits != 16) {
        AllFieldsValid = false;
        break;
      }
    }

    if (AllFieldsValid) {
      if (AllocSize <= 1 && SplitVTs.size() == 1 &&
          SplitVTs[0].getSizeInBits() == 8) {
        MIRBuilder.buildCopy(Regs.Ret_I8, VRegs[0]);
        emitRet({{Regs.Ret_I8, RegState::Implicit}});
        return true;
      }

      if (AllocSize <= 2) {
        SmallVector<unsigned, 2> Indices;
        for (unsigned I = 0; I < SplitVTs.size(); ++I)
          Indices.push_back(I);
        Register Word = packFieldsIntoWord(Indices);
        if (Word) {
          MIRBuilder.buildCopy(Regs.Ret_I16, Word);
          emitRet({{Regs.Ret_I16, RegState::Implicit}});
          return true;
        }
      }

      if (AllocSize <= 4) {
        // Split fields into Lo (first ≤2B) and Hi (remaining) groups.
        SmallVector<unsigned, 2> LoFields, HiFields;
        unsigned BytesSoFar = 0;
        for (unsigned I = 0; I < SplitVTs.size(); ++I) {
          unsigned FieldBytes = SplitVTs[I].getSizeInBits() / 8;
          if (BytesSoFar + FieldBytes <= 2)
            LoFields.push_back(I);
          else
            HiFields.push_back(I);
          BytesSoFar += FieldBytes;
        }

        if (!LoFields.empty() && !HiFields.empty()) {
          Register LoWord = packFieldsIntoWord(LoFields);
          Register HiWord = packFieldsIntoWord(HiFields);
          if (LoWord && HiWord) {
            MIRBuilder.buildCopy(Regs.Ret_I32_Lo, LoWord);
            MIRBuilder.buildCopy(Regs.Ret_I32_Hi, HiWord);
            emitRet({{Regs.Ret_I32_Lo, RegState::Implicit},
                     {Regs.Ret_I32_Hi, RegState::Implicit}});
            return true;
          }
        }
      }
    }

    // Fallback: pack via temporary stack slot for sub-byte fields
    // or other layouts that packFieldsIntoWord cannot handle.
    unsigned SlotSize = AllocSize <= 2 ? 2 : 4;
    MachineFrameInfo &MFI = MF.getFrameInfo();
    int FI = MFI.CreateStackObject(SlotSize, Align(1), false);

    // Store each field at its DataLayout offset.
    for (unsigned I = 0; I < VRegs.size(); ++I) {
      auto BaseAddr = MIRBuilder.buildFrameIndex(LLT::pointer(0, 16), FI);
      Register Addr;
      if (Offsets[I] == 0) {
        Addr = BaseAddr.getReg(0);
      } else {
        Addr = MIRBuilder
                   .buildPtrAdd(
                       LLT::pointer(0, 16), BaseAddr,
                       MIRBuilder.buildConstant(LLT::scalar(16), Offsets[I]))
                   .getReg(0);
      }
      unsigned FieldBits = SplitVTs[I].getSizeInBits();
      Register StoreReg = VRegs[I];
      LLT StoreTy;
      if (FieldBits < 8) {
        // Sub-byte field (e.g., i1): zero-extend to i8 for byte store.
        StoreTy = LLT::scalar(8);
        Register ExtReg = MRI.createGenericVirtualRegister(StoreTy);
        MIRBuilder.buildZExt(ExtReg, StoreReg);
        StoreReg = ExtReg;
      } else {
        StoreTy = LLT::scalar(FieldBits);
      }
      auto *MMO = MF.getMachineMemOperand(MachinePointerInfo::getStack(MF, 0),
                                          MachineMemOperand::MOStore, StoreTy,
                                          Align(1));
      MIRBuilder.buildStore(StoreReg, Addr, *MMO);
    }

    // Reload as scalar value(s) and copy to return registers.
    auto BaseAddr = MIRBuilder.buildFrameIndex(LLT::pointer(0, 16), FI);
    if (AllocSize <= 1) {
      Register RetReg = MRI.createGenericVirtualRegister(LLT::scalar(8));
      auto *MMO = MF.getMachineMemOperand(MachinePointerInfo::getStack(MF, 0),
                                          MachineMemOperand::MOLoad,
                                          LLT::scalar(8), Align(1));
      MIRBuilder.buildLoad(RetReg, BaseAddr, *MMO);
      MIRBuilder.buildCopy(Regs.Ret_I8, RetReg);
      emitRet({{Regs.Ret_I8, RegState::Implicit}});
    } else if (AllocSize <= 2) {
      Register RetReg = MRI.createGenericVirtualRegister(LLT::scalar(16));
      auto *MMO = MF.getMachineMemOperand(MachinePointerInfo::getStack(MF, 0),
                                          MachineMemOperand::MOLoad,
                                          LLT::scalar(16), Align(1));
      MIRBuilder.buildLoad(RetReg, BaseAddr, *MMO);
      MIRBuilder.buildCopy(Regs.Ret_I16, RetReg);
      emitRet({{Regs.Ret_I16, RegState::Implicit}});
    } else {
      // 3-4 bytes: use i32 return registers (two 16-bit words).
      Register LoReg = MRI.createGenericVirtualRegister(LLT::scalar(16));
      Register HiReg = MRI.createGenericVirtualRegister(LLT::scalar(16));
      auto *LoMMO = MF.getMachineMemOperand(MachinePointerInfo::getStack(MF, 0),
                                            MachineMemOperand::MOLoad,
                                            LLT::scalar(16), Align(1));
      MIRBuilder.buildLoad(LoReg, BaseAddr, *LoMMO);
      auto HiAddr =
          MIRBuilder.buildPtrAdd(LLT::pointer(0, 16), BaseAddr,
                                 MIRBuilder.buildConstant(LLT::scalar(16), 2));
      auto *HiMMO = MF.getMachineMemOperand(MachinePointerInfo::getStack(MF, 0),
                                            MachineMemOperand::MOLoad,
                                            LLT::scalar(16), Align(1));
      MIRBuilder.buildLoad(HiReg, HiAddr, *HiMMO);
      MIRBuilder.buildCopy(Regs.Ret_I32_Lo, LoReg);
      MIRBuilder.buildCopy(Regs.Ret_I32_Hi, HiReg);
      emitRet({{Regs.Ret_I32_Lo, RegState::Implicit},
               {Regs.Ret_I32_Hi, RegState::Implicit}});
    }
    return true;
  }

  unsigned BitWidth = RetTy->getPrimitiveSizeInBits();

  if (BitWidth == 0)
    BitWidth = 16; // Pointers

  if (BitWidth <= 8) {
    MIRBuilder.buildCopy(Regs.Ret_I8, VRegs[0]);
    emitRet({{Regs.Ret_I8, RegState::Implicit}});
  } else if (BitWidth <= 16) {
    MIRBuilder.buildCopy(Regs.Ret_I16, VRegs[0]);
    emitRet({{Regs.Ret_I16, RegState::Implicit}});
  } else if (BitWidth <= 32) {
    if (VRegs.size() >= 2) {
      MIRBuilder.buildCopy(Regs.Ret_I32_Lo, VRegs[0]); // Low word
      MIRBuilder.buildCopy(Regs.Ret_I32_Hi, VRegs[1]); // High word
    } else {
      // Single 32-bit vreg - need to unmerge
      Register LoReg = MRI.createGenericVirtualRegister(LLT::scalar(16));
      Register HiReg = MRI.createGenericVirtualRegister(LLT::scalar(16));
      MIRBuilder.buildUnmerge({LoReg, HiReg}, VRegs[0]);
      MIRBuilder.buildCopy(Regs.Ret_I32_Lo, LoReg);
      MIRBuilder.buildCopy(Regs.Ret_I32_Hi, HiReg);
    }
    emitRet({{Regs.Ret_I32_Lo, RegState::Implicit},
             {Regs.Ret_I32_Hi, RegState::Implicit}});
  } else {
    return false;
  }

  return true;
}

bool Z80CallLoweringCommon::lowerFormalArguments(
    MachineIRBuilder &MIRBuilder, const Function &F,
    ArrayRef<ArrayRef<Register>> VRegs, FunctionLoweringInfo &FLI) const {
  MachineFunction &MF = MIRBuilder.getMF();
  MachineBasicBlock &MBB = MIRBuilder.getMBB();
  MachineFrameInfo &MFI = MF.getFrameInfo();

  bool IsVarArg = F.isVarArg();
  CallingConv::ID CC = F.getCallingConv();

  if (F.arg_empty() && !IsVarArg)
    return true;

  const CallingConvRegs &Regs = getRegsForCC(CC);

  // SDCC argument passing convention.
  // sdcccall(1): up to 2 register params; 3rd+ on stack.
  // sdcccall(0): ALL parameters on stack.
  // For variadic functions: ALL arguments are passed on the stack.
  //
  // Stack layout at function entry (after prologue with frame pointer):
  //   IX+0 = saved IX (2 bytes, pushed by prologue) [Z80 only]
  //   IX+2 = return address (2 bytes, pushed by CALL)
  //   IX+4 = first stack argument
  MachineRegisterInfo &MRI = MF.getRegInfo();

  unsigned StackArgOffset = 0;
  bool HasStackArgs = false;

  unsigned RegParamCount = 0;
  FirstArgKind FirstKind = FIRST_NONE;

  // Handle sret demotion: when the return value can't fit in registers,
  // a hidden sret pointer is passed on the stack (SDCC convention).
  // The sret pointer is the first stack argument, before any regular stack
  // args. Regular arguments still use registers normally (sret does NOT consume
  // a register slot).
  if (!FLI.CanLowerReturn) {
    Register SRetReg = MRI.createGenericVirtualRegister(LLT::pointer(0, 16));
    int SRetFI = MFI.CreateFixedObject(2, 2 + StackArgOffset, true);
    auto SRetAddr = MIRBuilder.buildFrameIndex(LLT::pointer(0, 16), SRetFI);
    auto *SRetMMO = MF.getMachineMemOperand(
        MachinePointerInfo::getFixedStack(MF, SRetFI),
        MachineMemOperand::MOLoad, LLT::scalar(16), Align(1));
    MIRBuilder.buildLoad(SRetReg, SRetAddr, *SRetMMO);
    FLI.DemoteRegister = SRetReg;
    StackArgOffset += 2; // sret pointer takes 2 bytes on stack
    HasStackArgs = true;
  }

  unsigned ArgIdx = 0;
  for (const Argument &Arg : F.args()) {
    ArrayRef<Register> ArgVRegs = VRegs[ArgIdx];
    if (ArgVRegs.empty()) {
      ++ArgIdx;
      continue;
    }

    Register VReg = ArgVRegs[0];

    // Frontend-generated sret: when Clang demotes struct return at the
    // frontend level, the sret pointer appears as a regular arg with
    // StructRet attribute (FLI.CanLowerReturn is still true).
    // SDCC convention: sret pointer is always on the stack, never in a
    // register, and does NOT consume a register parameter slot.
    if (Arg.hasAttribute(Attribute::StructRet)) {
      int SRetFI = MFI.CreateFixedObject(2, 2 + StackArgOffset, true);
      auto SRetAddr = MIRBuilder.buildFrameIndex(LLT::pointer(0, 16), SRetFI);
      auto *SRetMMO = MF.getMachineMemOperand(
          MachinePointerInfo::getFixedStack(MF, SRetFI),
          MachineMemOperand::MOLoad, LLT::scalar(16), Align(1));
      MIRBuilder.buildLoad(VReg, SRetAddr, *SRetMMO);
      StackArgOffset += 2;
      HasStackArgs = true;
      ++ArgIdx;
      continue;
    }

    // Byval: struct bytes are on the stack. Provide a pointer (frame index)
    // to those bytes. Consumes a register parameter position (SDCC counts
    // struct args), so subsequent scalar args can't use that register slot.
    if (Arg.hasAttribute(Attribute::ByVal)) {
      classifyArg(Regs, RegParamCount, FirstKind, 64, IsVarArg, CC);
      Type *ByValTy = Arg.getParamByValType();
      unsigned ByValSize = MF.getDataLayout().getTypeAllocSize(ByValTy);
      int FI = MFI.CreateFixedObject(ByValSize, 2 + StackArgOffset, true);
      MIRBuilder.buildFrameIndex(VReg, FI);
      StackArgOffset += ByValSize;
      HasStackArgs = true;
      ++ArgIdx;
      continue;
    }

    Type *ArgTy = Arg.getType();
    unsigned BitWidth = ArgTy->getPrimitiveSizeInBits();

    if (BitWidth == 0)
      BitWidth = 16; // Pointers

    unsigned ByteWidth = (BitWidth + 7) / 8;

    ArgAssignment Assign =
        classifyArg(Regs, RegParamCount, FirstKind, BitWidth, IsVarArg, CC);
    if (Assign.InReg) {
      if (BitWidth <= 32 && Assign.PhysReg2.isValid()) {
        // i32: Hi:Lo pair
        MBB.addLiveIn(Assign.PhysReg);
        MBB.addLiveIn(Assign.PhysReg2);
        Register HiReg = MRI.createGenericVirtualRegister(LLT::scalar(16));
        Register LoReg = MRI.createGenericVirtualRegister(LLT::scalar(16));
        MIRBuilder.buildCopy(HiReg, Register(Assign.PhysReg));
        MIRBuilder.buildCopy(LoReg, Register(Assign.PhysReg2));
        MIRBuilder.buildMergeLikeInstr(VReg, {LoReg, HiReg});
      } else {
        MBB.addLiveIn(Assign.PhysReg);
        MIRBuilder.buildCopy(VReg, Register(Assign.PhysReg));
      }
    }

    if (!Assign.InReg) {
      // Stack argument: create a fixed stack object and load via frame index.
      // Fixed object offset is from incoming SP: +2 for return address.
      // eliminateFrameIndex will resolve to IX+d (hasFP) or SP-relative.
      HasStackArgs = true;

      if (BitWidth <= 16) {
        int FI = MFI.CreateFixedObject(ByteWidth, 2 + StackArgOffset, true);
        auto Addr = MIRBuilder.buildFrameIndex(LLT::pointer(0, 16), FI);
        auto *MMO = MF.getMachineMemOperand(
            MachinePointerInfo::getFixedStack(MF, FI),
            MachineMemOperand::MOLoad, LLT::scalar(BitWidth), Align(1));
        MIRBuilder.buildLoad(VReg, Addr, *MMO);
        StackArgOffset += ByteWidth; // i8=1 byte slot (SDCC packs i8)
      } else if (BitWidth <= 32) {
        // Load low 16 bits
        Register LoReg = MRI.createGenericVirtualRegister(LLT::scalar(16));
        int LoFI = MFI.CreateFixedObject(2, 2 + StackArgOffset, true);
        auto LoAddr = MIRBuilder.buildFrameIndex(LLT::pointer(0, 16), LoFI);
        auto *LoMMO = MF.getMachineMemOperand(
            MachinePointerInfo::getFixedStack(MF, LoFI),
            MachineMemOperand::MOLoad, LLT::scalar(16), Align(1));
        MIRBuilder.buildLoad(LoReg, LoAddr, *LoMMO);

        // Load high 16 bits
        Register HiReg = MRI.createGenericVirtualRegister(LLT::scalar(16));
        int HiFI = MFI.CreateFixedObject(2, 2 + StackArgOffset + 2, true);
        auto HiAddr = MIRBuilder.buildFrameIndex(LLT::pointer(0, 16), HiFI);
        auto *HiMMO = MF.getMachineMemOperand(
            MachinePointerInfo::getFixedStack(MF, HiFI),
            MachineMemOperand::MOLoad, LLT::scalar(16), Align(1));
        MIRBuilder.buildLoad(HiReg, HiAddr, *HiMMO);

        MIRBuilder.buildMergeLikeInstr(VReg, {LoReg, HiReg});
        StackArgOffset += 4;
      } else if (BitWidth % 16 == 0) {
        unsigned NumWords = BitWidth / 16;
        SmallVector<Register, 8> WordRegs;
        for (unsigned i = 0; i < NumWords; i++) {
          Register WordReg = MRI.createGenericVirtualRegister(LLT::scalar(16));
          int FI = MFI.CreateFixedObject(2, 2 + StackArgOffset + i * 2, true);
          auto Addr = MIRBuilder.buildFrameIndex(LLT::pointer(0, 16), FI);
          auto *MMO = MF.getMachineMemOperand(
              MachinePointerInfo::getFixedStack(MF, FI),
              MachineMemOperand::MOLoad, LLT::scalar(16), Align(1));
          MIRBuilder.buildLoad(WordReg, Addr, *MMO);
          WordRegs.push_back(WordReg);
        }
        MIRBuilder.buildMergeLikeInstr(VReg, WordRegs);
        StackArgOffset += NumWords * 2;
      } else {
        return false;
      }
    }

    ++ArgIdx;
  }

  // HasStackArgs is tracked but no longer forces frame address taken.
  // Fixed stack objects are resolved by eliminateFrameIndex for both
  // IX-based (hasFP) and SP-relative (!hasFP) modes.
  (void)HasStackArgs;

  // Record stack param bytes for callee-cleanup return.
  // If this function should clean up its own stack arguments (per SDCC rules),
  // lowerReturn will emit RET_CLEANUP instead of RET.
  if (StackArgOffset > 0) {
    const auto &STI = MF.getSubtarget<Z80Subtarget>();
    Type *FirstArgTy = F.arg_empty() ? nullptr : F.getArg(0)->getType();
    // Use the original return type for cleanup decision to match SDCC behavior.
    // For backend sret demote: F.getReturnType() returns the original type
    // (e.g. i64), giving RetBits=64 > 16 → caller cleanup.
    // For frontend sret: F.getReturnType() is void, but we recover the
    // original struct type from the sret parameter attribute.
    const DataLayout &DL = F.getDataLayout();
    Type *EffectiveRetTy = F.getReturnType();
    if (EffectiveRetTy->isVoidTy()) {
      for (const auto &Arg : F.args()) {
        if (Arg.hasStructRetAttr()) {
          Type *SRetTy = Arg.getParamStructRetType();
          if (SRetTy)
            EffectiveRetTy = SRetTy;
          break;
        }
      }
    }
    if (isCalleeCleanup(IsVarArg, EffectiveRetTy, FirstArgTy, STI.hasSM83(), CC,
                        DL)) {
      Z80FunctionInfo *FuncInfo = MF.getInfo<Z80FunctionInfo>();
      FuncInfo->StackParamBytes = StackArgOffset;
    }
  }

  // For variadic functions, create a fixed stack object pointing to the
  // first vararg. va_start will use this frame index.
  if (IsVarArg) {
    // StackArgOffset = total bytes of all fixed args on stack.
    // Varargs start at SPOffset = 2 (return addr) + StackArgOffset.
    // Z80 with FP: eliminateFrameIndex adds +2 for saved IX
    // (IX+4+StackArgOffset). SM83 / Z80 no-FP: SP-relative path, no saved IX
    // adjustment.
    int VarArgsFI = MFI.CreateFixedObject(2, 2 + StackArgOffset, true);
    Z80FunctionInfo *FuncInfo = MF.getInfo<Z80FunctionInfo>();
    FuncInfo->VarArgsStackIndex = VarArgsFI;
    MFI.setFrameAddressIsTaken(true);
  }

  return true;
}

bool Z80CallLoweringCommon::lowerCall(MachineIRBuilder &MIRBuilder,
                                      CallLoweringInfo &Info) const {
  MachineFunction &MF = MIRBuilder.getMF();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  CallingConv::ID CC = Info.CallConv;
  const CallingConvRegs &Regs = getRegsForCC(CC);

  // Handle sret demotion for libcalls with large return types (e.g., i64).
  // The generic createLibcall() doesn't call canLowerReturn(), so
  // Info.CanLowerReturn defaults to true even for types > 32 bits.
  //
  // SDCC convention: sret pointer is pushed on the stack as a hidden arg
  // (NOT passed in a register). Regular args still use registers normally.
  // We track the sret register separately instead of inserting into OrigArgs.
  Register SRetPtrReg;
  if (Info.CanLowerReturn && !Info.OrigRet.Ty->isVoidTy()) {
    const DataLayout &DL = MIRBuilder.getDataLayout();
    unsigned RetBytes = DL.getTypeAllocSize(Info.OrigRet.Ty);
    if (RetBytes > 4) {
      Info.CanLowerReturn = false;
      Type *RetTy = Info.OrigRet.Ty;
      unsigned AS = DL.getAllocaAddrSpace();
      LLT FramePtrTy = LLT::pointer(AS, DL.getPointerSizeInBits(AS));

      int FI = MF.getFrameInfo().CreateStackObject(
          DL.getTypeAllocSize(RetTy), DL.getPrefTypeAlign(RetTy), false);
      Register DemoteReg = MIRBuilder.buildFrameIndex(FramePtrTy, FI).getReg(0);
      SRetPtrReg = DemoteReg;
      Info.DemoteStackIndex = FI;
      Info.DemoteRegister = DemoteReg;
    }
  }

  // --- Pass 1: Classify arguments into register/stack, compute stack size ---
  FirstArgKind FirstKind = FIRST_NONE;
  unsigned RegParamCount = 0;
  unsigned StackArgBytes = 0;
  SmallVector<unsigned, 4> StackArgIndices;

  unsigned ArgIdx = 0;
  for (const ArgInfo &Arg : Info.OrigArgs) {
    if (Arg.Regs.empty()) {
      ++ArgIdx;
      continue;
    }

    // SDCC convention: sret pointer always goes on stack, never in a register.
    // It's pushed as the first stack arg (lowest address after return addr).
    // Don't let it consume a register slot.
    if (Arg.Flags[0].isSRet()) {
      StackArgIndices.push_back(ArgIdx);
      StackArgBytes += 2; // sret pointer is 16-bit
      ++ArgIdx;
      continue;
    }

    // SDCC convention: struct by-value (byval) always goes on stack as raw
    // bytes, never in registers. Consumes a register parameter slot (so
    // subsequent scalar args can't use that slot). Uses BitWidth>32 to ensure
    // classifyArg increments RegParamCount without assigning a register.
    // NOTE: variadic + byval combination is untested.
    if (Arg.Flags[0].isByVal()) {
      unsigned ByValSize = Arg.Flags[0].getByValSize();
      classifyArg(Regs, RegParamCount, FirstKind, 64, Info.IsVarArg, CC);
      StackArgIndices.push_back(ArgIdx);
      StackArgBytes += ByValSize;
      ++ArgIdx;
      continue;
    }

    unsigned BitWidth = Arg.Ty->getPrimitiveSizeInBits();
    if (BitWidth == 0)
      BitWidth = 16;

    ArgAssignment Assign = classifyArg(Regs, RegParamCount, FirstKind, BitWidth,
                                       Info.IsVarArg, CC);

    if (!Assign.InReg) {
      if (BitWidth % 16 != 0 && BitWidth > 16) {
        return false; // Non-16-bit-aligned wide types not supported
      }
      StackArgIndices.push_back(ArgIdx);
      unsigned ByteWidth = (BitWidth + 7) / 8;
      StackArgBytes += ByteWidth; // i8=1 byte, i16=2, i32=4 (SDCC packs i8)
    }

    ++ArgIdx;
  }

  // Add sret pointer bytes to stack arg count (SDCC pushes sret on stack)
  if (SRetPtrReg.isValid())
    StackArgBytes += 2;

  // --- Pass 2: Emit code ---

  // Emit ADJCALLSTACKDOWN
  MIRBuilder.buildInstr(Z80::ADJCALLSTACKDOWN).addImm(StackArgBytes).addImm(0);

  // Push stack arguments in right-to-left order (last arg pushed first,
  // so first stack arg ends up at lowest address = IX+4 in callee)
  for (auto I = StackArgIndices.rbegin(), E = StackArgIndices.rend(); I != E;
       ++I) {
    const ArgInfo &Arg = Info.OrigArgs[*I];
    Register VReg = Arg.Regs[0];

    // Byval: copy struct bytes from source pointer to stack.
    // Push from highest offset to lowest so that lowest offset ends up
    // at the lowest stack address (matching SDCC's memory layout).
    if (Arg.Flags[0].isByVal()) {
      unsigned ByValSize = Arg.Flags[0].getByValSize();
      unsigned Off = ByValSize;

      // Handle odd trailing byte
      if (Off & 1) {
        Off--;
        Register AddrReg =
            MRI.createGenericVirtualRegister(LLT::pointer(0, 16));
        MIRBuilder.buildPtrAdd(AddrReg, VReg,
                               MIRBuilder.buildConstant(LLT::scalar(16), Off));
        Register ByteReg = MRI.createGenericVirtualRegister(LLT::scalar(8));
        auto *MMO = MF.getMachineMemOperand(MachinePointerInfo(),
                                            MachineMemOperand::MOLoad,
                                            LLT::scalar(8), Align(1));
        MIRBuilder.buildLoad(ByteReg, AddrReg, *MMO);
        MIRBuilder.buildCopy(Z80::A, ByteReg);
        MIRBuilder.buildInstr(Z80::PUSH_AF);
        MIRBuilder.buildInstr(Z80::INC_SP);
      }

      // Push 16-bit words from high to low
      while (Off >= 2) {
        Off -= 2;
        Register AddrReg;
        if (Off == 0) {
          AddrReg = VReg; // No offset needed
        } else {
          AddrReg = MRI.createGenericVirtualRegister(LLT::pointer(0, 16));
          MIRBuilder.buildPtrAdd(
              AddrReg, VReg, MIRBuilder.buildConstant(LLT::scalar(16), Off));
        }
        Register WordReg = MRI.createGenericVirtualRegister(LLT::scalar(16));
        auto *MMO = MF.getMachineMemOperand(MachinePointerInfo(),
                                            MachineMemOperand::MOLoad,
                                            LLT::scalar(16), Align(1));
        MIRBuilder.buildLoad(WordReg, AddrReg, *MMO);
        MIRBuilder.buildCopy(Z80::HL, WordReg);
        MIRBuilder.buildInstr(Z80::PUSH_HL);
      }
      continue;
    }

    unsigned BitWidth = Arg.Ty->getPrimitiveSizeInBits();
    if (BitWidth == 0)
      BitWidth = 16;

    if (BitWidth <= 8) {
      // Push i8 as 1 byte: PUSH AF + INC SP (matches SDCC's push af;inc sp)
      MIRBuilder.buildCopy(Z80::A, VReg);
      MIRBuilder.buildInstr(Z80::PUSH_AF);
      MIRBuilder.buildInstr(Z80::INC_SP);
    } else if (BitWidth <= 16) {
      MIRBuilder.buildCopy(Z80::HL, VReg);
      MIRBuilder.buildInstr(Z80::PUSH_HL);
    } else if (BitWidth <= 32) {
      // i32: push high word first, then low word
      Register LoReg = MRI.createGenericVirtualRegister(LLT::scalar(16));
      Register HiReg = MRI.createGenericVirtualRegister(LLT::scalar(16));
      MIRBuilder.buildUnmerge({LoReg, HiReg}, VReg);
      MIRBuilder.buildCopy(Z80::HL, HiReg);
      MIRBuilder.buildInstr(Z80::PUSH_HL);
      MIRBuilder.buildCopy(Z80::HL, LoReg);
      MIRBuilder.buildInstr(Z80::PUSH_HL);
    } else {
      // Wide types (i64, i128, etc.): push N words, highest first
      unsigned NumWords = BitWidth / 16;
      SmallVector<Register, 8> WordRegs;
      for (unsigned i = 0; i < NumWords; i++)
        WordRegs.push_back(MRI.createGenericVirtualRegister(LLT::scalar(16)));
      MIRBuilder.buildUnmerge(WordRegs, VReg);
      for (int i = NumWords - 1; i >= 0; i--) {
        MIRBuilder.buildCopy(Z80::HL, WordRegs[i]);
        MIRBuilder.buildInstr(Z80::PUSH_HL);
      }
    }
  }

  // Push sret pointer on stack (after regular stack args, so it ends up
  // at the lowest address = first stack arg in callee's frame).
  if (SRetPtrReg.isValid()) {
    MIRBuilder.buildCopy(Z80::HL, SRetPtrReg);
    MIRBuilder.buildInstr(Z80::PUSH_HL);
  }

  // Emit register argument copies and track which physical registers are used.
  RegParamCount = 0;
  FirstKind = FIRST_NONE;
  SmallVector<Register, 4> ArgRegs;

  if (!Info.IsVarArg) {
    for (const ArgInfo &Arg : Info.OrigArgs) {
      if (Arg.Regs.empty())
        continue;

      // Skip sret args - they go on stack, not in registers
      if (Arg.Flags[0].isSRet())
        continue;

      // Byval args go on stack but consume a register parameter position
      if (Arg.Flags[0].isByVal()) {
        classifyArg(Regs, RegParamCount, FirstKind, 64, Info.IsVarArg, CC);
        continue;
      }

      Register VReg = Arg.Regs[0];
      unsigned BitWidth = Arg.Ty->getPrimitiveSizeInBits();
      if (BitWidth == 0)
        BitWidth = 16;

      ArgAssignment Assign = classifyArg(Regs, RegParamCount, FirstKind,
                                         BitWidth, Info.IsVarArg, CC);
      if (Assign.InReg) {
        if (BitWidth <= 32 && Assign.PhysReg2.isValid()) {
          // i32: split into Hi and Lo
          Register LoReg = MRI.createGenericVirtualRegister(LLT::scalar(16));
          Register HiReg = MRI.createGenericVirtualRegister(LLT::scalar(16));
          MIRBuilder.buildUnmerge({LoReg, HiReg}, VReg);
          MIRBuilder.buildCopy(Assign.PhysReg, HiReg);
          MIRBuilder.buildCopy(Assign.PhysReg2, LoReg);
          ArgRegs.push_back(Assign.PhysReg);
          ArgRegs.push_back(Assign.PhysReg2);
        } else {
          MIRBuilder.buildCopy(Assign.PhysReg, VReg);
          ArgRegs.push_back(Assign.PhysReg);
        }
      }
    }
  }

  // Build the call instruction with dynamic implicit uses for argument regs
  MachineInstrBuilder CallMI;
  if (Info.Callee.isReg()) {
    MIRBuilder.buildCopy(Regs.IndirectCallReg, Info.Callee.getReg());
    CallMI = MIRBuilder.buildInstr(Regs.IndirectCallOpc);
  } else {
    CallMI = MIRBuilder.buildInstr(Z80::CALL_nn);
    CallMI.add(Info.Callee);
  }

  // Add implicit uses for each physical register used as an argument
  for (Register Reg : ArgRegs)
    CallMI.addUse(Reg, RegState::Implicit);

  // Compute callee-cleanup amount before emitting return value handling.
  // For callee-cleanup calls, ADJCALLSTACKUP must be emitted BEFORE sret loads
  // because the callee has already restored SP — PEI's SPAdj must reflect this
  // before resolving sret frame indices.
  unsigned CalleeCleanupBytes = 0;
  if (StackArgBytes > 0) {
    const auto &STI = MF.getSubtarget<Z80Subtarget>();
    const DataLayout &DL = MF.getFunction().getDataLayout();
    // Use the original return type for cleanup decision per SDCC convention:
    // i64 return → 64 > 16 bits → caller cleanup (Z80).
    // For frontend sret (Info.OrigRet.Ty is void, sret on first arg),
    // recover the original struct type from the sret attribute.
    Type *EffectiveRetTy = Info.OrigRet.Ty;
    if (EffectiveRetTy->isVoidTy() && Info.CB) {
      if (Info.CB->hasStructRetAttr()) {
        Type *SRetTy = Info.CB->getParamStructRetType(0);
        if (SRetTy)
          EffectiveRetTy = SRetTy;
      }
    }
    Type *FirstArgTy = Info.OrigArgs.empty() ? nullptr : Info.OrigArgs[0].Ty;
    if (isCalleeCleanup(Info.IsVarArg, EffectiveRetTy, FirstArgTy,
                        STI.hasSM83(), CC, DL))
      CalleeCleanupBytes = StackArgBytes;
  }

  // For callee-cleanup calls, emit ADJCALLSTACKUP IMMEDIATELY after the CALL,
  // before any return value handling. The callee has already restored SP, so
  // PEI's SPAdj must be decremented before any instructions that might use
  // frame indices (sret loads, or register allocator-inserted spills of return
  // value registers).
  bool EmittedAdjUp = false;
  if (CalleeCleanupBytes > 0) {
    MIRBuilder.buildInstr(Z80::ADJCALLSTACKUP)
        .addImm(StackArgBytes)
        .addImm(CalleeCleanupBytes);
    EmittedAdjUp = true;
  }

  // Return values: i8->Ret_I8, i16->Ret_I16, i32->Ret_I32_Hi:Ret_I32_Lo
  if (!Info.OrigRet.Ty->isVoidTy() && !Info.OrigRet.Regs.empty()) {
    if (!Info.CanLowerReturn) {
      // sret: load return value from stack object (written by callee)
      insertSRetLoads(MIRBuilder, Info.OrigRet.Ty, Info.OrigRet.Regs,
                      Info.DemoteRegister, Info.DemoteStackIndex);
    } else if (Info.OrigRet.Ty->isAggregateType()) {
      const DataLayout &DL = MF.getDataLayout();
      unsigned AllocSize = DL.getTypeAllocSize(Info.OrigRet.Ty);

      SmallVector<EVT, 4> SplitVTs;
      SmallVector<uint64_t, 4> Offsets;
      ComputeValueVTs(*getTLI(), DL, Info.OrigRet.Ty, SplitVTs, nullptr,
                      &Offsets, 0);

      // Unpack aggregate return registers into fields using field-per-slot
      // assignment (symmetric with pack logic in lowerReturn).
      auto unpackFieldsFromWord = [&](Register Word,
                                      ArrayRef<unsigned> FieldIndices) -> bool {
        if (FieldIndices.size() == 1) {
          unsigned Idx = FieldIndices[0];
          unsigned Bits = SplitVTs[Idx].getSizeInBits();
          if (Bits == 16) {
            MIRBuilder.buildCopy(Info.OrigRet.Regs[Idx], Word);
            return true;
          }
          if (Bits == 8) {
            MIRBuilder.buildTrunc(Info.OrigRet.Regs[Idx], Word);
            return true;
          }
          return false;
        }
        if (FieldIndices.size() == 2) {
          unsigned Idx0 = FieldIndices[0], Idx1 = FieldIndices[1];
          if (SplitVTs[Idx0].getSizeInBits() == 8 &&
              SplitVTs[Idx1].getSizeInBits() == 8) {
            MIRBuilder.buildUnmerge(
                {Info.OrigRet.Regs[Idx0], Info.OrigRet.Regs[Idx1]}, Word);
            return true;
          }
          return false;
        }
        return false;
      };

      bool AllFieldsValid = true;
      for (unsigned I = 0; I < SplitVTs.size(); ++I) {
        unsigned Bits = SplitVTs[I].getSizeInBits();
        if (Bits != 8 && Bits != 16) {
          AllFieldsValid = false;
          break;
        }
      }

      bool Unpacked = false;
      if (AllFieldsValid) {
        if (AllocSize <= 1 && SplitVTs.size() == 1 &&
            SplitVTs[0].getSizeInBits() == 8) {
          MIRBuilder.buildCopy(Info.OrigRet.Regs[0], Register(Regs.Ret_I8));
          Unpacked = true;
        } else if (AllocSize <= 2) {
          if (SplitVTs.size() == 1 && SplitVTs[0].getSizeInBits() == 16) {
            // Single i16 field: direct copy, no intermediate vreg.
            MIRBuilder.buildCopy(Info.OrigRet.Regs[0], Register(Regs.Ret_I16));
            Unpacked = true;
          } else {
            Register RetReg =
                MRI.createGenericVirtualRegister(LLT::scalar(16));
            MIRBuilder.buildCopy(RetReg, Register(Regs.Ret_I16));
            SmallVector<unsigned, 2> Indices;
            for (unsigned I = 0; I < SplitVTs.size(); ++I)
              Indices.push_back(I);
            Unpacked = unpackFieldsFromWord(RetReg, Indices);
          }
        } else if (AllocSize <= 4) {
          SmallVector<unsigned, 2> LoFields, HiFields;
          unsigned BytesSoFar = 0;
          for (unsigned I = 0; I < SplitVTs.size(); ++I) {
            unsigned FieldBytes = SplitVTs[I].getSizeInBits() / 8;
            if (BytesSoFar + FieldBytes <= 2)
              LoFields.push_back(I);
            else
              HiFields.push_back(I);
            BytesSoFar += FieldBytes;
          }

          if (!LoFields.empty() && !HiFields.empty()) {
            Register LoWord = MRI.createGenericVirtualRegister(LLT::scalar(16));
            Register HiWord = MRI.createGenericVirtualRegister(LLT::scalar(16));
            MIRBuilder.buildCopy(LoWord, Register(Regs.Ret_I32_Lo));
            MIRBuilder.buildCopy(HiWord, Register(Regs.Ret_I32_Hi));
            Unpacked = unpackFieldsFromWord(LoWord, LoFields) &&
                       unpackFieldsFromWord(HiWord, HiFields);
          }
        }
      }

      if (!Unpacked) {
        // Fallback: extract via temporary stack slot for irregular layouts.
        unsigned SlotSize = AllocSize <= 2 ? 2 : 4;
        MachineFrameInfo &MFI = MF.getFrameInfo();
        int FI = MFI.CreateStackObject(SlotSize, Align(1), false);

        auto BaseAddr = MIRBuilder.buildFrameIndex(LLT::pointer(0, 16), FI);
        if (AllocSize <= 1) {
          Register TmpReg = MRI.createGenericVirtualRegister(LLT::scalar(8));
          MIRBuilder.buildCopy(TmpReg, Register(Regs.Ret_I8));
          auto *MMO = MF.getMachineMemOperand(
              MachinePointerInfo::getStack(MF, 0), MachineMemOperand::MOStore,
              LLT::scalar(8), Align(1));
          MIRBuilder.buildStore(TmpReg, BaseAddr, *MMO);
        } else if (AllocSize <= 2) {
          Register TmpReg = MRI.createGenericVirtualRegister(LLT::scalar(16));
          MIRBuilder.buildCopy(TmpReg, Register(Regs.Ret_I16));
          auto *MMO = MF.getMachineMemOperand(
              MachinePointerInfo::getStack(MF, 0), MachineMemOperand::MOStore,
              LLT::scalar(16), Align(1));
          MIRBuilder.buildStore(TmpReg, BaseAddr, *MMO);
        } else {
          Register LoReg = MRI.createGenericVirtualRegister(LLT::scalar(16));
          Register HiReg = MRI.createGenericVirtualRegister(LLT::scalar(16));
          MIRBuilder.buildCopy(LoReg, Register(Regs.Ret_I32_Lo));
          MIRBuilder.buildCopy(HiReg, Register(Regs.Ret_I32_Hi));
          auto *LoMMO = MF.getMachineMemOperand(
              MachinePointerInfo::getStack(MF, 0), MachineMemOperand::MOStore,
              LLT::scalar(16), Align(1));
          MIRBuilder.buildStore(LoReg, BaseAddr, *LoMMO);
          auto HiAddr = MIRBuilder.buildPtrAdd(
              LLT::pointer(0, 16), BaseAddr,
              MIRBuilder.buildConstant(LLT::scalar(16), 2));
          auto *HiMMO = MF.getMachineMemOperand(
              MachinePointerInfo::getStack(MF, 0), MachineMemOperand::MOStore,
              LLT::scalar(16), Align(1));
          MIRBuilder.buildStore(HiReg, HiAddr, *HiMMO);
        }

        for (unsigned I = 0; I < Info.OrigRet.Regs.size(); ++I) {
          auto FIAddr = MIRBuilder.buildFrameIndex(LLT::pointer(0, 16), FI);
          Register Addr;
          if (Offsets[I] == 0) {
            Addr = FIAddr.getReg(0);
          } else {
            Addr =
                MIRBuilder
                    .buildPtrAdd(LLT::pointer(0, 16), FIAddr,
                                 MIRBuilder.buildConstant(LLT::scalar(16),
                                                          Offsets[I]))
                    .getReg(0);
          }
          unsigned FieldBits = SplitVTs[I].getSizeInBits();
          LLT LoadTy = FieldBits < 8 ? LLT::scalar(8) : LLT::scalar(FieldBits);
          if (FieldBits < 8) {
            Register LoadReg =
                MRI.createGenericVirtualRegister(LLT::scalar(8));
            auto *MMO = MF.getMachineMemOperand(
                MachinePointerInfo::getStack(MF, 0), MachineMemOperand::MOLoad,
                LoadTy, Align(1));
            MIRBuilder.buildLoad(LoadReg, Addr, *MMO);
            MIRBuilder.buildTrunc(Info.OrigRet.Regs[I], LoadReg);
          } else {
            auto *MMO = MF.getMachineMemOperand(
                MachinePointerInfo::getStack(MF, 0), MachineMemOperand::MOLoad,
                LoadTy, Align(1));
            MIRBuilder.buildLoad(Info.OrigRet.Regs[I], Addr, *MMO);
          }
        }
      }
    } else {
      Register VReg = Info.OrigRet.Regs[0];
      unsigned BitWidth = Info.OrigRet.Ty->getPrimitiveSizeInBits();
      if (BitWidth == 0)
        BitWidth = 16;

      if (BitWidth <= 8) {
        MIRBuilder.buildCopy(VReg, Register(Regs.Ret_I8));
      } else if (BitWidth <= 16) {
        MIRBuilder.buildCopy(VReg, Register(Regs.Ret_I16));
      } else if (BitWidth <= 32) {
        Register LoReg = MRI.createGenericVirtualRegister(LLT::scalar(16));
        Register HiReg = MRI.createGenericVirtualRegister(LLT::scalar(16));
        MIRBuilder.buildCopy(LoReg, Register(Regs.Ret_I32_Lo));
        MIRBuilder.buildCopy(HiReg, Register(Regs.Ret_I32_Hi));
        MIRBuilder.buildMergeLikeInstr(VReg, {LoReg, HiReg});
      } else {
        return false;
      }
    }
  }

  // Emit ADJCALLSTACKUP if not already emitted above (callee-cleanup).
  if (!EmittedAdjUp) {
    MIRBuilder.buildInstr(Z80::ADJCALLSTACKUP)
        .addImm(StackArgBytes)
        .addImm(CalleeCleanupBytes);
  }

  return true;
}

bool Z80CallLoweringCommon::canLowerReturn(MachineFunction &MF,
                                           CallingConv::ID CC,
                                           SmallVectorImpl<BaseArgInfo> &Outs,
                                           bool IsVarArg) const {
  // Return up to 4 bytes (32 bits) in registers.
  // Use DataLayout alloc sizes so that sub-byte types like i1 correctly
  // count as 1 byte each (getPrimitiveSizeInBits would undercount).
  const DataLayout &DL = MF.getDataLayout();
  unsigned TotalBytes = 0;
  for (const auto &Out : Outs)
    TotalBytes += DL.getTypeAllocSize(Out.Ty);
  return TotalBytes <= 4;
}
