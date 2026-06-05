//===-- Z80InstrCost.h - Z80 Instruction Cost structure ---------*- C++ -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the definition of the Z80InstrCost class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_Z80_Z80INSTRCOST_H
#define LLVM_LIB_TARGET_Z80_Z80INSTRCOST_H

#include "llvm/CodeGen/MachineFunction.h"
#include <cstdint>

namespace llvm {

class Z80InstrCost {
public:
  enum class Mode { PreferBytes, PreferCycles, Average };

  Z80InstrCost() : Bytes(0), Cycles(0) {}

  Z80InstrCost(int32_t Bytes, int32_t Cycles)
      : Z80InstrCost(Bytes, Cycles, 256) {}

  friend Z80InstrCost operator+(Z80InstrCost Left, const Z80InstrCost &Right) {
    return Z80InstrCost(Left.Bytes + Right.Bytes, Left.Cycles + Right.Cycles,
                        1);
  }

  Z80InstrCost &operator+=(const Z80InstrCost &Right) {
    this->Bytes += Right.Bytes;
    this->Cycles += Right.Cycles;
    return *this;
  }

  friend Z80InstrCost operator-(Z80InstrCost Left, const Z80InstrCost &Right) {
    return Z80InstrCost(Left.Bytes - Right.Bytes, Left.Cycles - Right.Cycles,
                        1);
  }

  Z80InstrCost &operator-=(const Z80InstrCost &Right) {
    this->Bytes -= Right.Bytes;
    this->Cycles -= Right.Cycles;
    return *this;
  }

  friend Z80InstrCost operator*(Z80InstrCost Left, int Right) {
    return Z80InstrCost(Left.Bytes * Right, Left.Cycles * Right, 1);
  }

  friend Z80InstrCost operator/(Z80InstrCost Left, int Right) {
    return Z80InstrCost(Left.Bytes / Right, Left.Cycles / Right, 1);
  }

  int64_t value(Mode Mode = Mode::Average) const;

  static Mode getModeFor(const MachineFunction &MF);

private:
  Z80InstrCost(int32_t Bytes, int32_t Cycles, int Multiplier)
      : Bytes(Bytes * Multiplier), Cycles(Cycles * Multiplier) {}

  int32_t Bytes, Cycles;
};

} // namespace llvm

#endif // not LLVM_LIB_TARGET_Z80_Z80INSTRCOST_H
