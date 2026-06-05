//===-- Z80InstrCost.cpp - Z80 Instruction Cost structure -------*- C++ -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains additional helpers for the Z80InstrCost class.
//
//===----------------------------------------------------------------------===//

#include "Z80InstrCost.h"

#include "llvm/IR/Function.h"

using namespace llvm;

namespace llvm {

int64_t Z80InstrCost::value(Mode Mode) const {
  switch (Mode) {
  case Mode::PreferBytes:
    return ((int64_t)Bytes << 32) + Cycles;
  case Mode::PreferCycles:
    return ((int64_t)Cycles << 32) + Bytes;
  case Mode::Average:
    return Bytes + Cycles;
  }
}

Z80InstrCost::Mode Z80InstrCost::getModeFor(const MachineFunction &MF) {
  if (MF.getFunction().hasMinSize())
    return Mode::PreferBytes;
  if (MF.getFunction().hasOptSize() || MF.getFunction().hasOptNone())
    return Mode::Average;
  return Mode::PreferCycles;
}

} // namespace llvm
