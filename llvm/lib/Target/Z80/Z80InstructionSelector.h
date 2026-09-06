//===-- Z80InstructionSelector.h - Z80 Instruction Selector -----*- C++ -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the Z80 instruction selector.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_Z80_Z80INSTRUCTIONSELECTOR_H
#define LLVM_LIB_TARGET_Z80_Z80INSTRUCTIONSELECTOR_H

#include "Z80RegisterBankInfo.h"
#include "Z80Subtarget.h"
#include "Z80TargetMachine.h"
#include "llvm/CodeGen/GlobalISel/InstructionSelector.h"

namespace llvm {

InstructionSelector *createZ80InstructionSelector(const Z80TargetMachine &TM,
                                                  Z80Subtarget &STI,
                                                  Z80RegisterBankInfo &RBI);

} // namespace llvm

#endif // not LLVM_LIB_TARGET_Z80_Z80INSTRUCTIONSELECTOR_H
