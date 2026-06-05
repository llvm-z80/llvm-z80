//===-- Z80ISelLowering.cpp - Z80 DAG Lowering Implementation -------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the interfaces that Z80 uses to lower LLVM code into a
// selection DAG.
//
//===----------------------------------------------------------------------===//

#include "Z80ISelLowering.h"

#include "llvm/ADT/StringSwitch.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/GlobalISel/MachineIRBuilder.h"
#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/ErrorHandling.h"

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80.h"
#include "Z80InstrInfo.h"
#include "Z80RegisterInfo.h"
#include "Z80Subtarget.h"
#include "Z80TargetMachine.h"

using namespace llvm;

Z80TargetLowering::Z80TargetLowering(const Z80TargetMachine &TM,
                                     const Z80Subtarget &STI)
    : TargetLowering(TM, STI) {
  // Register classes for Z80
  addRegisterClass(MVT::i8, &Z80::GR8RegClass);
  addRegisterClass(MVT::i16, &Z80::GR16RegClass);
  computeRegisterProperties(STI.getRegisterInfo());

  // Boolean results from ICMP are always 0 or 1.  This ensures that when
  // the legalizer widens s1 booleans (e.g. G_BRCOND condition) to s8, it
  // uses G_ZEXT instead of G_ANYEXT, preserving only bit 0.
  setBooleanContents(ZeroOrOneBooleanContent);

  // Stack pointer register
  setStackPointerRegisterToSaveRestore(Z80::SP);

  // Z80 has limited jump table support
  setMaximumJumpTableSize(std::min(256u, getMaximumJumpTableSize()));

  // Z80 has no conditional move instruction, so SELECT is always expanded
  // to a branch sequence. Prefer branches over selects since they avoid
  // computing both sides of the conditional.
  PredictableSelectIsExpensive = true;
}

MVT Z80TargetLowering::getRegisterType(MVT VT) const {
  // Z80 has 8-bit and 16-bit registers
  if (VT.getSizeInBits() > 16)
    return MVT::i8; // Split larger values into bytes
  if (VT.getSizeInBits() > 8)
    return MVT::i16;
  return TargetLowering::getRegisterType(VT);
}

unsigned
Z80TargetLowering::getNumRegisters(LLVMContext &Context, EVT VT,
                                   std::optional<MVT> RegisterVT) const {
  if (VT.getSizeInBits() > 16)
    return VT.getSizeInBits() / 8;
  if (VT.getSizeInBits() > 8)
    return 1; // One 16-bit register
  return TargetLowering::getNumRegisters(Context, VT, RegisterVT);
}



TargetLowering::ConstraintType
Z80TargetLowering::getConstraintType(StringRef Constraint) const {
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
    default:
      break;
    case 'a': // Accumulator
    case 'b': // B register
    case 'c': // C register
    case 'd': // D register
    case 'e': // E register
    case 'h': // H register
    case 'l': // L register
      return C_Register;
    case 'R':
      return C_RegisterClass;
    }
  }
  return TargetLowering::getConstraintType(Constraint);
}

std::pair<unsigned, const TargetRegisterClass *>
Z80TargetLowering::getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                                                StringRef Constraint,
                                                MVT VT) const {
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
    default:
      break;
    case 'r':
      if (VT == MVT::i16)
        return std::make_pair(0U, &Z80::GR16RegClass);
      return std::make_pair(0U, &Z80::GR8RegClass);
    case 'R':
      return std::make_pair(0U, &Z80::GR8RegClass);
    case 'a':
      return std::make_pair(Z80::A, &Z80::GR8RegClass);
    case 'b':
      return std::make_pair(Z80::B, &Z80::GR8RegClass);
    case 'c':
      return std::make_pair(Z80::C, &Z80::GR8RegClass);
    case 'd':
      return std::make_pair(Z80::D, &Z80::GR8RegClass);
    case 'e':
      return std::make_pair(Z80::E, &Z80::GR8RegClass);
    case 'h':
      return std::make_pair(Z80::H, &Z80::GR8RegClass);
    case 'l':
      return std::make_pair(Z80::L, &Z80::GR8RegClass);
    }
  }

  return TargetLowering::getRegForInlineAsmConstraint(TRI, Constraint, VT);
}

bool Z80TargetLowering::isLegalAddressingMode(const DataLayout &DL,
                                              const AddrMode &AM, Type *Ty,
                                              unsigned AS,
                                              Instruction *I) const {
  // Z80 supports simple base + offset addressing
  if (AM.BaseGV && !AM.HasBaseReg && AM.Scale == 0)
    return true;
  if (!AM.BaseGV && AM.HasBaseReg && AM.Scale == 0)
    return true;
  return false;
}

bool Z80TargetLowering::isTruncateFree(Type *FromTy, Type *ToTy) const {
  // Truncation is free on Z80 (just use low byte), but only when actually
  // narrowing.  Returning true for same-size types causes LLVM passes
  // (LoopStrengthReduce, IndVarSimplify) to attempt zero-width
  // truncations/extensions, triggering assertions.
  if (!FromTy->isIntegerTy() || !ToTy->isIntegerTy())
    return false;
  return FromTy->getIntegerBitWidth() > ToTy->getIntegerBitWidth();
}

bool Z80TargetLowering::isTruncateFree(LLT FromTy, LLT ToTy,
                                       LLVMContext &Ctx) const {
  return FromTy.isScalar() && ToTy.isScalar() &&
         FromTy.getSizeInBits() > ToTy.getSizeInBits();
}

bool Z80TargetLowering::isZExtFree(Type *FromTy, Type *ToTy) const {
  return false; // Zero extension requires explicit operations
}

bool Z80TargetLowering::isZExtFree(LLT FromTy, LLT ToTy,
                                   LLVMContext &Ctx) const {
  return false;
}

MachineBasicBlock *
Z80TargetLowering::EmitInstrWithCustomInserter(MachineInstr &MI,
                                               MachineBasicBlock *MBB) const {
  // TODO: Implement custom instruction insertion
  llvm_unreachable("EmitInstrWithCustomInserter not implemented for Z80");
}
