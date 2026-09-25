//===-- Z80ISelLowering.h - Z80 DAG Lowering Interface ----------*- C++ -*-===//
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

#ifndef LLVM_LIB_TARGET_Z80_Z80ISELLOWERING_H
#define LLVM_LIB_TARGET_Z80_Z80ISELLOWERING_H

#include "llvm/CodeGen/TargetLowering.h"

#include "llvm/Target/TargetMachine.h"

namespace llvm {

class Z80Subtarget;
class Z80TargetMachine;

class Z80TargetLowering : public TargetLowering {
public:
  Z80TargetLowering(const Z80TargetMachine &TM, const Z80Subtarget &STI);

  bool isSelectSupported(SelectSupportKind /*kind*/) const override {
    return false;
  }

  // Soft-float comparison libcalls (__eqdf2, __nedf2, __ltdf2, ...) follow the
  // GCC ABI and return a C `int`. On Z80 `int` is 16-bit, but TargetLowering's
  // default getCmpLibcallReturnType() is i32. Leaving it at i32 makes the
  // caller read a 32-bit result (HL:DE) from a routine that only defines the
  // low 16 bits, so the high word is callee garbage and corrupts the boolean
  // (e.g. `a == a` intermittently returns false). Override to i16 to match the
  // runtime shims.
  MVT::SimpleValueType getCmpLibcallReturnType() const override {
    return MVT::i16;
  }

  // While integer division isn't "cheap", long division is not all that much
  // slower than long multiplication, and the division->multiplication
  // optimization this disables performs multiplciation at double the width,
  // which is extraordinarily more expensive.
  bool isIntDivCheap(EVT VT, AttributeList Attr) const override { return true; }

  bool areJTsAllowed(const Function *Fn) const override {
    return !Fn->getFnAttribute("no-jump-tables").getValueAsBool();
  }

  /// The wide compares this target legalizes to produce a zero-or-one
  /// boolean, which nothing else can know about a target opcode.
  void computeKnownBitsForTargetInstr(GISelValueTracking &Analysis, Register R,
                                      KnownBits &Known,
                                      const APInt &DemandedElts,
                                      const MachineRegisterInfo &MRI,
                                      unsigned Depth = 0) const override;

  ConstraintType getConstraintType(StringRef Constraint) const override;

  MVT getRegisterTypeForCallingConv(LLVMContext &Context, CallingConv::ID CC,
                                    EVT VT) const override;

  unsigned getNumRegistersForCallingConv(LLVMContext &Context,
                                         CallingConv::ID CC,
                                         EVT VT) const override;

  std::pair<unsigned, const TargetRegisterClass *>
  getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                               StringRef Constraint, MVT VT) const override;

  bool isLegalAddressingMode(const DataLayout &DL, const AddrMode &AM, Type *Ty,
                             unsigned AddrSpace,
                             Instruction *I = nullptr) const override;

  bool isTruncateFree(Type *FromTy, Type *ToTy) const override;
  bool isTruncateFree(LLT FromTy, LLT ToTy, LLVMContext &Ctx) const override;

  bool isZExtFree(Type *FromTy, Type *ToTy) const override;
  bool isZExtFree(LLT FromTy, LLT ToTy, LLVMContext &Ctx) const override;

  EVT getOptimalMemOpType(LLVMContext &Context, const MemOp &Op,
                          const AttributeList &FuncAttributes) const override {
    return MVT::i8;
  }

  LLT getOptimalMemOpLLT(const MemOp &Op,
                         const AttributeList &FuncAttributes) const override {
    return LLT::scalar(8);
  }

  MachineBasicBlock *
  EmitInstrWithCustomInserter(MachineInstr &MI,
                              MachineBasicBlock *MBB) const override;
};

} // namespace llvm

#endif // not LLVM_LIB_TARGET_Z80_Z80ISELLOWERING_H
