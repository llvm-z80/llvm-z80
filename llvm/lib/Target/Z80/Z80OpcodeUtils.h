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

/// Get the opcode for LD dest,src (8-bit register copy).
/// Returns 0 if either register is not in {A,B,C,D,E,H,L}.
inline unsigned getLD8RegOpcode(Register Dest, Register Src) {
  static const unsigned LDOpcodes[7][7] = {
      // src:   A            B            C            D            E H L
      /*A*/ {Z80::LD_A_A, Z80::LD_A_B, Z80::LD_A_C, Z80::LD_A_D, Z80::LD_A_E,
             Z80::LD_A_H, Z80::LD_A_L},
      /*B*/
      {Z80::LD_B_A, Z80::LD_B_B, Z80::LD_B_C, Z80::LD_B_D, Z80::LD_B_E,
       Z80::LD_B_H, Z80::LD_B_L},
      /*C*/
      {Z80::LD_C_A, Z80::LD_C_B, Z80::LD_C_C, Z80::LD_C_D, Z80::LD_C_E,
       Z80::LD_C_H, Z80::LD_C_L},
      /*D*/
      {Z80::LD_D_A, Z80::LD_D_B, Z80::LD_D_C, Z80::LD_D_D, Z80::LD_D_E,
       Z80::LD_D_H, Z80::LD_D_L},
      /*E*/
      {Z80::LD_E_A, Z80::LD_E_B, Z80::LD_E_C, Z80::LD_E_D, Z80::LD_E_E,
       Z80::LD_E_H, Z80::LD_E_L},
      /*H*/
      {Z80::LD_H_A, Z80::LD_H_B, Z80::LD_H_C, Z80::LD_H_D, Z80::LD_H_E,
       Z80::LD_H_H, Z80::LD_H_L},
      /*L*/
      {Z80::LD_L_A, Z80::LD_L_B, Z80::LD_L_C, Z80::LD_L_D, Z80::LD_L_E,
       Z80::LD_L_H, Z80::LD_L_L},
  };
  int DstIdx = gr8RegToIndex(Dest);
  int SrcIdx = gr8RegToIndex(Src);
  if (DstIdx >= 0 && SrcIdx >= 0)
    return LDOpcodes[DstIdx][SrcIdx];
  return 0;
}

/// Get LD (HL),r opcode. Returns 0 for unsupported registers.
inline unsigned getStoreHLindOpcode(Register Reg) {
  static const unsigned Opcodes[] = {
      Z80::LD_HLind_A, Z80::LD_HLind_B, Z80::LD_HLind_C, Z80::LD_HLind_D,
      Z80::LD_HLind_E, Z80::LD_HLind_H, Z80::LD_HLind_L,
  };
  int Idx = gr8RegToIndex(Reg);
  return Idx >= 0 ? Opcodes[Idx] : 0;
}

/// Get LD r,(HL) opcode. Returns 0 for unsupported registers.
inline unsigned getLoadHLindOpcode(Register Reg) {
  static const unsigned Opcodes[] = {
      Z80::LD_A_HLind, Z80::LD_B_HLind, Z80::LD_C_HLind, Z80::LD_D_HLind,
      Z80::LD_E_HLind, Z80::LD_H_HLind, Z80::LD_L_HLind,
  };
  int Idx = gr8RegToIndex(Reg);
  return Idx >= 0 ? Opcodes[Idx] : 0;
}

} // namespace Z80
} // namespace llvm

#endif // LLVM_LIB_TARGET_Z80_Z80OPCODEUTILS_H
