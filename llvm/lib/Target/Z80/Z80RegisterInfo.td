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

// Flag bits
def sub_c : Z80SubRegIndex<1>;      // Carry (bit 0)
def sub_n : Z80SubRegIndex<1>;      // Add/Subtract (bit 1)
def sub_pv : Z80SubRegIndex<1>;     // Parity/Overflow (bit 2)
def sub_h : Z80SubRegIndex<1>;      // Half-carry (bit 4)
def sub_z : Z80SubRegIndex<1>;      // Zero (bit 6)
def sub_s : Z80SubRegIndex<1>;      // Sign (bit 7)

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
def BC : Z80RegPair<30, "bc", C, B>;
def DE : Z80RegPair<31, "de", E, D>;
def HL : Z80RegPair<32, "hl", L, H>;
def AF : Z80RegPair<33, "af", F, A>;

// Index registers
def IX : Z80RegPair<34, "ix", IXL, IXH>;
def IY : Z80RegPair<35, "iy", IYL, IYH>;

// Stack pointer (no accessible sub-registers)
def SP : Z80Reg<36, "sp">;

// Program counter (not directly accessible but needed)
def PC : Z80Reg<37, "pc">;

// Shadow register pairs
def AFp : Z80RegPair<40, "alt_af", Fp, Ap>;
def BCp : Z80RegPair<41, "alt_bc", Cp, Bp>;
def DEp : Z80RegPair<42, "alt_de", Ep, Dp>;
def HLp : Z80RegPair<43, "alt_hl", Lp, Hp>;

//===----------------------------------------------------------------------===//
//  Flag pseudo-registers (for instruction patterns)
//===----------------------------------------------------------------------===//

def CF : Z80Reg<50, "cf">;   // Carry flag
def NF : Z80Reg<51, "nf">;   // Add/Subtract flag
def PVF : Z80Reg<52, "pvf">; // Parity/Overflow flag
def HF : Z80Reg<53, "hf">;   // Half-carry flag
def ZF : Z80Reg<54, "zf">;   // Zero flag
def SF : Z80Reg<55, "sf">;   // Sign flag

// Standalone FLAGS register for dependency tracking by RA/scheduler.
// NOT a sub-register of AF to avoid implicit-def $af issues
// (see known-issues.md: LDrn/CBRotate implicit-def problem).
def FLAGS : Z80Reg<56, "flags">;

//===----------------------------------------------------------------------===//
//  Register classes
//===----------------------------------------------------------------------===//

class Z80RegClass<list<ValueType> regTypes, int alignment, dag regList>
     : RegisterClass<"Z80", regTypes, alignment, regList>;

class Z80Reg1Class<dag RegList>: Z80RegClass<[i1], 8, RegList> {
  let RegInfos = RegInfoByHwMode<[DefaultMode], [RegInfo<1, 8, 8>]>;
}
class Z80Reg8Class<dag RegList>: Z80RegClass<[i8], 8, RegList>;
class Z80Reg16Class<dag RegList>: Z80RegClass<[i16], 8, RegList>;

//===----------------------------------------------------------------------===//
//  8-bit register classes
//===----------------------------------------------------------------------===//

// Main 8-bit GPRs: B, C, D, E, H, L, A
// These can be used with most 8-bit operations
def GR8 : Z80Reg8Class<(add B, C, D, E, H, L, A)>;

//===----------------------------------------------------------------------===//
//  16-bit register classes
//===----------------------------------------------------------------------===//

// General 16-bit registers: BC, DE, HL
def GR16 : Z80Reg16Class<(add BC, DE, HL)>;

// BC and DE only (for register scavenging temp in address computation)
def GR16_BCDE : Z80Reg16Class<(add BC, DE)>;

// 16-bit registers for PUSH/POP: BC, DE, HL, AF
def GR16_AF : Z80Reg16Class<(add BC, DE, HL, AF)>;

// 16-bit registers for ADD HL,rr: BC, DE, HL, SP
def GR16_SP : Z80Reg16Class<(add BC, DE, HL, SP)>;

// 16-bit registers including SP (for LD rr,nn etc)
def GR16_All : Z80Reg16Class<(add BC, DE, HL, SP)>;

// Index registers only
def IR16 : Z80Reg16Class<(add IX, IY)>;

// HL or index register (for indirect addressing)
def HLI : Z80Reg16Class<(add HL, IX, IY)>;

// Pointer registers (can hold addresses)
def Ptr16 : Z80Reg16Class<(add BC, DE, HL, IX, IY, SP)>;

// Stack pointer only
def SPc : Z80Reg16Class<(add SP)>;

// All 16-bit registers for PUSH/POP with indices
def Push16 : Z80Reg16Class<(add BC, DE, HL, AF, IX, IY)>;

//===----------------------------------------------------------------------===//
//  Flag register classes
//===----------------------------------------------------------------------===//

def Fc : Z80Reg8Class<(add F)>;

let isAllocatable = false in {
  def Flags : Z80Reg1Class<(add CF, NF, PVF, HF, ZF, SF)>;
}

// Carry flag class (for ADC, SBC, etc)
def Cc : Z80Reg1Class<(add CF)>;

// Condition code register class for FLAGS dependency tracking.
// Non-allocatable, CopyCost=-1 prevents copying (like ARM's CCR for CPSR).
def CCR : RegisterClass<"Z80", [i8], 8, (add FLAGS)> {
  let CopyCost = -1;
  let isAllocatable = 0;
}

//===----------------------------------------------------------------------===//
//  Combined register classes
//===----------------------------------------------------------------------===//

// All 8-bit storage locations
def Anyi8 : Z80Reg8Class<(add GR8)>;

// All 16-bit storage locations
def Anyi16 : Z80Reg16Class<(add BC, DE, HL, IX, IY, SP)>;

//===----------------------------------------------------------------------===//
//  Fake registers for multi-word values
//===----------------------------------------------------------------------===//

def Fake : Z80Reg<100, "fake">;
let isAllocatable = false in {
  def Fakei32 : Z80RegClass<[i32], 8, (add Fake)>;
  def Fakei64 : Z80RegClass<[i64], 8, (add Fake)>;
}

