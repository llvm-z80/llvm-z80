//===-- Z80OpcodeUtils.h - Z80 Register/Opcode Utilities --------*- C++ -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Shared utility functions for mapping Z80 registers to opcode table indices,
// PUSH/POP opcodes, and LD r,r' opcodes. Used by Z80InstrInfo, Z80RegisterInfo,
// and Z80FrameLowering to avoid duplicating the same switch statements.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_Z80_Z80OPCODEUTILS_H
#define LLVM_LIB_TARGET_Z80_Z80OPCODEUTILS_H

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "llvm/CodeGen/Register.h"

namespace llvm {
namespace Z80 {

/// Map a GR8 physical register to a table index.
/// A=0, B=1, C=2, D=3, E=4, H=5, L=6. Returns -1 for invalid registers.
inline int gr8RegToIndex(Register Reg) {
  switch (Reg.id()) {
  case Z80::A:
    return 0;
  case Z80::B:
    return 1;
  case Z80::C:
    return 2;
  case Z80::D:
    return 3;
  case Z80::E:
    return 4;
  case Z80::H:
    return 5;
  case Z80::L:
    return 6;
  default:
    return -1;
  }
}

/// Get PUSH opcode for a 16-bit register. Returns 0 for unsupported registers.
inline unsigned getPushOpcode(Register Reg) {
  switch (Reg.id()) {
  case Z80::BC:
    return Z80::PUSH_BC;
  case Z80::DE:
    return Z80::PUSH_DE;
  case Z80::HL:
    return Z80::PUSH_HL;
  case Z80::AF:
    return Z80::PUSH_AF;
  case Z80::IX:
    return Z80::PUSH_IX;
  case Z80::IY:
    return Z80::PUSH_IY;
  default:
    return 0;
  }
}

/// Get POP opcode for a 16-bit register. Returns 0 for unsupported registers.
inline unsigned getPopOpcode(Register Reg) {
  switch (Reg.id()) {
  case Z80::BC:
    return Z80::POP_BC;
  case Z80::DE:
    return Z80::POP_DE;
  case Z80::HL:
    return Z80::POP_HL;
  case Z80::AF:
    return Z80::POP_AF;
  case Z80::IX:
    return Z80::POP_IX;
  case Z80::IY:
    return Z80::POP_IY;
  default:
    return 0;
  }
}

/// Whether the three-bit register field of an opcode can name \p Reg.
inline bool isEncodableGR8(Register Reg) { return gr8RegToIndex(Reg) >= 0; }

/// Whether `LD dst, src` exists as a one-byte register copy: both registers
/// must be ones the opcode's three-bit register fields can name.
inline bool canLD8(Register Dst, Register Src) {
  return isEncodableGR8(Dst) && isEncodableGR8(Src);
}

} // namespace Z80
} // namespace llvm

#endif // LLVM_LIB_TARGET_Z80_Z80OPCODEUTILS_H
