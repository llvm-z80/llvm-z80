//===- Z80InlineAsmLowering.h -----------------------------------*- C++ -*-===//
//
// Part of the LLVM-Z80 Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file describes how to lower LLVM inline asm to machine code INLINEASM.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_Z80_Z80INLINEASMLOWERING_H
#define LLVM_LIB_TARGET_Z80_Z80INLINEASMLOWERING_H

#include "Z80ISelLowering.h"
#include "llvm/CodeGen/GlobalISel/InlineAsmLowering.h"

namespace llvm {

class Z80TargetLowering;

class Z80InlineAsmLowering : public InlineAsmLowering {
public:
  Z80InlineAsmLowering(Z80TargetLowering *TLI);

  bool
  lowerAsmOperandForConstraint(Value *Val, StringRef Constraint,
                               std::vector<MachineOperand> &Ops,
                               MachineIRBuilder &MIRBuilder) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_Z80_Z80INLINEASMLOWERING_H
