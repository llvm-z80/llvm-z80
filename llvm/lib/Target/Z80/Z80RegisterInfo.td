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
// Z80 Registers:
//   8-bit: A, B, C, D, E, H, L, F, I, R, IXH, IXL, IYH, IYL (undocumented)
//   16-bit pairs: AF, BC, DE, HL, IX, IY, SP, PC
//   Shadow set: A', B', C', D', E', H', L', F' (AF', BC', DE', HL')
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//  Subregister indices
//===----------------------------------------------------------------------===//

class Z80SubRegIndex<int size, int offset = 0>: SubRegIndex<size, offset> {
  let Namespace = "Z80";
}

// Low byte of a 16-bit register pair
def sub_lo : Z80SubRegIndex<8>;

// High byte of a 16-bit register pair
def sub_hi : Z80SubRegIndex<8, 8>;

//===----------------------------------------------------------------------===//
//  Register definitions
//===----------------------------------------------------------------------===//

class Z80Reg<bits<16> num, string name, list<string> altNames = []>
  : Register<name>, DwarfRegNum<[num]> {
  field bits<16> Num = num;
  let HWEncoding = num;
  let Namespace = "Z80";
  let AltNames = altNames;
}

//===----------------------------------------------------------------------===//
//  8-bit registers
//===----------------------------------------------------------------------===//

// Main 8-bit registers
// Register encoding follows Z80 convention: B=0, C=1, D=2, E=3, H=4, L=5, (HL)=6, A=7
def B : Z80Reg<0, "b">;
def C : Z80Reg<1, "c">;
def D : Z80Reg<2, "d">;
def E : Z80Reg<3, "e">;
def H : Z80Reg<4, "h">;
def L : Z80Reg<5, "l">;
def A : Z80Reg<7, "a">;

// Flags register
def F : Z80Reg<8, "f">;

// Index register halves (undocumented on Z80, absent on SM83).
// Gated by FeatureUndocumented — reserved when the feature is disabled.
def IXH : Z80Reg<9, "ixh">;
def IXL : Z80Reg<10, "ixl">;
def IYH : Z80Reg<11, "iyh">;
def IYL : Z80Reg<12, "iyl">;

// Special 8-bit registers
def I : Z80Reg<13, "i">;    // Interrupt vector base
def R : Z80Reg<14, "r">;    // Memory refresh counter

// Shadow registers (alternate register set)
// Note: Using 'alt_' prefix instead of apostrophe suffix to avoid
// TableGen AsmMatcher issues with apostrophes in C++ character literals.
// Traditional Z80 syntax uses a', b', etc. but this causes code generation issues.
def Ap : Z80Reg<20, "alt_a">;
def Fp : Z80Reg<21, "alt_f">;
def Bp : Z80Reg<22, "alt_b">;
def Cp : Z80Reg<23, "alt_c">;
def Dp : Z80Reg<24, "alt_d">;
def Ep : Z80Reg<25, "alt_e">;
def Hp : Z80Reg<26, "alt_h">;
def Lp : Z80Reg<27, "alt_l">;

//===----------------------------------------------------------------------===//
//  16-bit register pairs
//===----------------------------------------------------------------------===//

class Z80RegPair<bits<16> num, string name, Register lo, Register hi>
    : Z80Reg<num, name> {
  let SubRegs = [lo, hi];
  let SubRegIndices = [sub_lo, sub_hi];
  let CoveredBySubRegs = 1;
}

// Main register pairs (encoding: BC=0, DE=1, HL=2, SP/AF=3)
// The pair field of an opcode names BC=0, DE=1, HL=2 and then either SP or
// AF depending on the instruction, so HWEncoding carries that rather than the
// DWARF number the register is defined with.
def BC : Z80RegPair<30, "bc", C, B> { let HWEncoding = 0; }
def DE : Z80RegPair<31, "de", E, D> { let HWEncoding = 1; }
def HL : Z80RegPair<32, "hl", L, H> { let HWEncoding = 2; }
def AF : Z80RegPair<33, "af", F, A> { let HWEncoding = 3; }

// Index registers
// A DD or FD prefix makes the HL encoding mean IX or IY.
def IX : Z80RegPair<34, "ix", IXL, IXH> { let HWEncoding = 2; }
def IY : Z80RegPair<35, "iy", IYL, IYH> { let HWEncoding = 2; }

// Stack pointer (no accessible sub-registers)
def SP : Z80Reg<36, "sp"> { let HWEncoding = 3; }

// Program counter (not directly accessible but needed)
def PC : Z80Reg<37, "pc">;

// Shadow register pairs
def AFp : Z80RegPair<40, "alt_af", Fp, Ap>;
def BCp : Z80RegPair<41, "alt_bc", Cp, Bp>;
def DEp : Z80RegPair<42, "alt_de", Ep, Dp>;
def HLp : Z80RegPair<43, "alt_hl", Lp, Hp>;

//===----------------------------------------------------------------------===//
//  Flags pseudo-register
//===----------------------------------------------------------------------===//

// Standalone FLAGS register for dependency tracking by RA/scheduler.
// NOT a sub-register of AF to avoid implicit-def $af issues
// (LDrn/CBRotate implicit-def problem).
def FLAGS : Z80Reg<56, "flags">;

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

// Condition code register class for FLAGS dependency tracking.
// Non-allocatable, CopyCost=-1 prevents copying (like ARM's CCR for CPSR).
def CCR : RegisterClass<"Z80", [i8], 8, (add FLAGS)> {
  let CopyCost = -1;
  let isAllocatable = 0;
}
