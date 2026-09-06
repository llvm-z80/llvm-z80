//===-- Z80MachineFunctionInfo.h - Z80 machine function info ----*- C++ -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares Z80-specific per-machine-function information.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_Z80_Z80MACHINEFUNCTIONINFO_H
#define LLVM_LIB_TARGET_Z80_Z80MACHINEFUNCTIONINFO_H

#include "llvm/CodeGen/MachineFunction.h"

namespace llvm {

class Z80Subtarget;

struct Z80FunctionInfo : public MachineFunctionInfo {
  Z80FunctionInfo(const Function &F, const Z80Subtarget *STI) {}

  int VarArgsStackIndex = -1;
  /// Bytes of stack parameters that callee must clean up (0 = caller cleanup).
  /// Set by lowerFormalArguments, used by lowerReturn to emit RET_CLEANUP.
  /// Convention-agnostic: works for __sdcccall(1) and future __z88dk_callee.
  unsigned StackParamBytes = 0;

  /// Size of the callee-saved register area (bytes pushed by
  /// spillCalleeSavedRegisters). Used by emitPrologue/emitEpilogue to
  /// compute the local-only stack allocation size.
  unsigned CalleeSavedFrameSize = 0;
  unsigned getCalleeSavedFrameSize() const { return CalleeSavedFrameSize; }
  void setCalleeSavedFrameSize(unsigned S) { CalleeSavedFrameSize = S; }
};

} // namespace llvm

#endif // not LLVM_LIB_TARGET_Z80_Z80MACHINEFUNCTIONINFO_H
