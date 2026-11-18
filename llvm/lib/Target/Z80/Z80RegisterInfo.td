//===-- Z80RegisterInfo.td - Z80 Register defs -------------*- tablegen -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes the Z80 register file, defining the registers themselves,
// their subregisters, and register classes.
//
//===----------------------------------------------------------------------===//

let Namespace = "Z80" in {
  def sub_lo : SubRegIndex<8>;
  def sub_hi : SubRegIndex<8, 8>;
}

class Z80Reg<string name, bits<16> enc> : Register<name> {
  let HWEncoding = enc;
  let Namespace = "Z80";
}

//===----------------------------------------------------------------------===//
//  8-bit registers
//===----------------------------------------------------------------------===//

class Z80Reg8<string name, bits<16> enc> : Z80Reg<name, enc>;

// Main 8-bit registers
def B : Z80Reg8<"b", 0>;
def C : Z80Reg8<"c", 1>;
def D : Z80Reg8<"d", 2>;
def E : Z80Reg8<"e", 3>;
def H : Z80Reg8<"h", 4>;
def L : Z80Reg8<"l", 5>;
def A : Z80Reg8<"a", 7>;

// Flags register
def F : Z80Reg8<"f", 0>;

// Special 8-bit registers
def I : Z80Reg8<"i", 0>;    // Interrupt vector base
def R : Z80Reg8<"r", 0>;    // Memory refresh counter

// Shadow 8-bit registers
// Note: Using 'alt_' prefix instead of apostrophe suffix to avoid
// TableGen AsmMatcher issues with apostrophes in C++ character literals.
// Traditional Z80 syntax uses a', b', etc. but this causes code generation issues.
def Ap : Z80Reg8<"alt_a", 0>;
def Fp : Z80Reg8<"alt_f", 0>;
def Bp : Z80Reg8<"alt_b", 0>;
def Cp : Z80Reg8<"alt_c", 0>;
def Dp : Z80Reg8<"alt_d", 0>;
def Ep : Z80Reg8<"alt_e", 0>;
def Hp : Z80Reg8<"alt_h", 0>;
def Lp : Z80Reg8<"alt_l", 0>;

// Undocumented index register halves
def IXH : Z80Reg8<"ixh", 0>;
def IXL : Z80Reg8<"ixl", 0>;
def IYH : Z80Reg8<"iyh", 0>;
def IYL : Z80Reg8<"iyl", 0>;

//===----------------------------------------------------------------------===//
//  16-bit register pairs
//===----------------------------------------------------------------------===//

class Z80Reg16<string name, bits<16> enc, int dwarf>
    : Z80Reg<name, enc>, DwarfRegNum<[dwarf]>;

class Z80RegPair<string name, bits<16> enc, int dwarf, Register lo, Register hi>
    : Z80Reg16<name, enc, dwarf> {
  let SubRegs = [lo, hi];
  let SubRegIndices = [sub_lo, sub_hi];
  let CoveredBySubRegs = 1;
}

// Main 16-bit register pairs
def BC : Z80RegPair<"bc", 0, 1, C, B>;
def DE : Z80RegPair<"de", 1, 2, E, D>;
def HL : Z80RegPair<"hl", 2, 3, L, H>;
def AF : Z80RegPair<"af", 3, 0, F, A>;

// Index registers
def IX : Z80RegPair<"ix", 2, 6, IXL, IXH>;
def IY : Z80RegPair<"iy", 2, 7, IYL, IYH>;

// Stack pointer
def SP : Z80Reg16<"sp", 3, 4>;

// Program counter
def PC : Z80Reg16<"pc", 0, 5>;

// Shadow register pairs
def AFp : Z80RegPair<"alt_af", 0, 8, Fp, Ap>;
def BCp : Z80RegPair<"alt_bc", 0, 9, Cp, Bp>;
def DEp : Z80RegPair<"alt_de", 0, 10, Ep, Dp>;
def HLp : Z80RegPair<"alt_hl", 0, 11, Lp, Hp>;

//===----------------------------------------------------------------------===//
//  Register classes
//===----------------------------------------------------------------------===//

class Z80RegClass<list<ValueType> regTypes, int alignment, dag regList>
     : RegisterClass<"Z80", regTypes, alignment, regList>;

class Z80Reg8Class<dag RegList>: Z80RegClass<[i8], 8, RegList>;
class Z80Reg16Class<dag RegList>: Z80RegClass<[i16], 8, RegList>;

//===----------------------------------------------------------------------===//
//  8-bit register classes
//===----------------------------------------------------------------------===//

// Main 8-bit GPRs: B, C, D, E, H, L, A
// These can be used with most 8-bit operations
def GR8 : Z80Reg8Class<(add B, C, D, E, H, L, A)>;

// The accumulator alone, for the operands of instructions that read or write
// a byte in A. Its ranges have no choice of register, so they are allocated
// first, local or not, and the values around them see where A is taken
// before picking theirs.
def Ac : Z80Reg8Class<(add A)> {
  let AllocationPriority = 1;
  let GlobalPriority = true;
}

//===----------------------------------------------------------------------===//
//  16-bit register classes
//===----------------------------------------------------------------------===//

// General 16-bit registers: BC, DE, HL
def GR16 : Z80Reg16Class<(add BC, DE, HL)>;

// BC and DE, for a pair used alongside HL
def GR16_BCDE : Z80Reg16Class<(add BC, DE)>;

// Index registers only
def IR16 : Z80Reg16Class<(add IX, IY)>;

// HL, IX and IY, the pairs SP is copied to and from
def HLI : Z80Reg16Class<(add HL, IX, IY)>;

// Every pair a 16-bit value can be kept in.
// We leave SP out here because, with SP in this class, a copy to or from SP
// could be folded into a spill of SP, and SP cannot be spilled.
def Any16 : Z80Reg16Class<(add BC, DE, HL, IX, IY)>;

// Stack pointer only
def SPc : Z80Reg16Class<(add SP)>;

// AF alone. Interrupts save AF, and inline asm can name AF.
def AFc : Z80Reg16Class<(add AF)>;

//===----------------------------------------------------------------------===//
//  Flag register classes
//===----------------------------------------------------------------------===//

// F alone. Inline asm can name F.
def Fc : Z80Reg8Class<(add F)>;
