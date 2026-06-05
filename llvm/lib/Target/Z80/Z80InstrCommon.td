//===-- Z80InstrCommon.td - Instructions shared with SM83 ---*- tablegen -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Instructions common to Z80 and SM83 (Game Boy CPU).
// These use unprefixed or CB-prefixed opcodes that are identical on both CPUs.
// Z80-only instructions (ED/DD/FD prefix, exchange, block transfer, I/O,
// sign/parity conditions, DJNZ) remain in Z80InstrInfo.td.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//  8-bit Load Group
//===----------------------------------------------------------------------===//

// LD r,r' - Load register to register
// Opcode: 0b01dddsss where ddd=dest, sss=src
def LD_B_B : Inst8<"ld b,b", 0x40> { let Uses = [B]; let Defs = [B]; }
def LD_B_C : Inst8<"ld b,c", 0x41> { let Uses = [C]; let Defs = [B]; }
def LD_B_D : Inst8<"ld b,d", 0x42> { let Uses = [D]; let Defs = [B]; }
def LD_B_E : Inst8<"ld b,e", 0x43> { let Uses = [E]; let Defs = [B]; }
def LD_B_H : Inst8<"ld b,h", 0x44> { let Uses = [H]; let Defs = [B]; }
def LD_B_L : Inst8<"ld b,l", 0x45> { let Uses = [L]; let Defs = [B]; }
def LD_B_A : Inst8<"ld b,a", 0x47> { let Uses = [A]; let Defs = [B]; }

def LD_C_B : Inst8<"ld c,b", 0x48> { let Uses = [B]; let Defs = [C]; }
def LD_C_C : Inst8<"ld c,c", 0x49> { let Uses = [C]; let Defs = [C]; }
def LD_C_D : Inst8<"ld c,d", 0x4A> { let Uses = [D]; let Defs = [C]; }
def LD_C_E : Inst8<"ld c,e", 0x4B> { let Uses = [E]; let Defs = [C]; }
def LD_C_H : Inst8<"ld c,h", 0x4C> { let Uses = [H]; let Defs = [C]; }
def LD_C_L : Inst8<"ld c,l", 0x4D> { let Uses = [L]; let Defs = [C]; }
def LD_C_A : Inst8<"ld c,a", 0x4F> { let Uses = [A]; let Defs = [C]; }

def LD_D_B : Inst8<"ld d,b", 0x50> { let Uses = [B]; let Defs = [D]; }
def LD_D_C : Inst8<"ld d,c", 0x51> { let Uses = [C]; let Defs = [D]; }
def LD_D_D : Inst8<"ld d,d", 0x52> { let Uses = [D]; let Defs = [D]; }
def LD_D_E : Inst8<"ld d,e", 0x53> { let Uses = [E]; let Defs = [D]; }
def LD_D_H : Inst8<"ld d,h", 0x54> { let Uses = [H]; let Defs = [D]; }
def LD_D_L : Inst8<"ld d,l", 0x55> { let Uses = [L]; let Defs = [D]; }
def LD_D_A : Inst8<"ld d,a", 0x57> { let Uses = [A]; let Defs = [D]; }

def LD_E_B : Inst8<"ld e,b", 0x58> { let Uses = [B]; let Defs = [E]; }
def LD_E_C : Inst8<"ld e,c", 0x59> { let Uses = [C]; let Defs = [E]; }
def LD_E_D : Inst8<"ld e,d", 0x5A> { let Uses = [D]; let Defs = [E]; }
def LD_E_E : Inst8<"ld e,e", 0x5B> { let Uses = [E]; let Defs = [E]; }
def LD_E_H : Inst8<"ld e,h", 0x5C> { let Uses = [H]; let Defs = [E]; }
def LD_E_L : Inst8<"ld e,l", 0x5D> { let Uses = [L]; let Defs = [E]; }
def LD_E_A : Inst8<"ld e,a", 0x5F> { let Uses = [A]; let Defs = [E]; }

def LD_H_B : Inst8<"ld h,b", 0x60> { let Uses = [B]; let Defs = [H]; }
def LD_H_C : Inst8<"ld h,c", 0x61> { let Uses = [C]; let Defs = [H]; }
def LD_H_D : Inst8<"ld h,d", 0x62> { let Uses = [D]; let Defs = [H]; }
def LD_H_E : Inst8<"ld h,e", 0x63> { let Uses = [E]; let Defs = [H]; }
def LD_H_H : Inst8<"ld h,h", 0x64> { let Uses = [H]; let Defs = [H]; }
def LD_H_L : Inst8<"ld h,l", 0x65> { let Uses = [L]; let Defs = [H]; }
def LD_H_A : Inst8<"ld h,a", 0x67> { let Uses = [A]; let Defs = [H]; }

def LD_L_B : Inst8<"ld l,b", 0x68> { let Uses = [B]; let Defs = [L]; }
def LD_L_C : Inst8<"ld l,c", 0x69> { let Uses = [C]; let Defs = [L]; }
def LD_L_D : Inst8<"ld l,d", 0x6A> { let Uses = [D]; let Defs = [L]; }
def LD_L_E : Inst8<"ld l,e", 0x6B> { let Uses = [E]; let Defs = [L]; }
def LD_L_H : Inst8<"ld l,h", 0x6C> { let Uses = [H]; let Defs = [L]; }
def LD_L_L : Inst8<"ld l,l", 0x6D> { let Uses = [L]; let Defs = [L]; }
def LD_L_A : Inst8<"ld l,a", 0x6F> { let Uses = [A]; let Defs = [L]; }

def LD_A_B : Inst8<"ld a,b", 0x78> { let Uses = [B]; let Defs = [A]; }
def LD_A_C : Inst8<"ld a,c", 0x79> { let Uses = [C]; let Defs = [A]; }
def LD_A_D : Inst8<"ld a,d", 0x7A> { let Uses = [D]; let Defs = [A]; }
def LD_A_E : Inst8<"ld a,e", 0x7B> { let Uses = [E]; let Defs = [A]; }
def LD_A_H : Inst8<"ld a,h", 0x7C> { let Uses = [H]; let Defs = [A]; }
def LD_A_L : Inst8<"ld a,l", 0x7D> { let Uses = [L]; let Defs = [A]; }
def LD_A_A : Inst8<"ld a,a", 0x7F> { let Uses = [A]; let Defs = [A]; }

// LD r,n - Load immediate to register
def LD_B_n : LDrn<REG_B> { let Defs = [B]; }
def LD_C_n : LDrn<REG_C> { let Defs = [C]; }
def LD_D_n : LDrn<REG_D> { let Defs = [D]; }
def LD_E_n : LDrn<REG_E> { let Defs = [E]; }
def LD_H_n : LDrn<REG_H> { let Defs = [H]; }
def LD_L_n : LDrn<REG_L> { let Defs = [L]; }
def LD_A_n : Inst16<"ld a,", 0x3E, Imm8Mode> { let Defs = [A]; }

// LD r,(HL) - Load from memory at HL
let mayLoad = true, mayStore = false, hasSideEffects = false in {
def LD_B_HLind : Inst8<"ld b,(hl)", 0x46> { let Uses = [HL]; let Defs = [B]; }
def LD_C_HLind : Inst8<"ld c,(hl)", 0x4E> { let Uses = [HL]; let Defs = [C]; }
def LD_D_HLind : Inst8<"ld d,(hl)", 0x56> { let Uses = [HL]; let Defs = [D]; }
def LD_E_HLind : Inst8<"ld e,(hl)", 0x5E> { let Uses = [HL]; let Defs = [E]; }
def LD_H_HLind : Inst8<"ld h,(hl)", 0x66> { let Uses = [HL]; let Defs = [H]; }
def LD_L_HLind : Inst8<"ld l,(hl)", 0x6E> { let Uses = [HL]; let Defs = [L]; }
def LD_A_HLind : Inst8<"ld a,(hl)", 0x7E> { let Uses = [HL]; let Defs = [A]; }
}

// LD (HL),r - Store to memory at HL
let mayLoad = false, mayStore = true, hasSideEffects = false in {
def LD_HLind_B : Inst8<"ld (hl),b", 0x70> { let Uses = [HL, B]; }
def LD_HLind_C : Inst8<"ld (hl),c", 0x71> { let Uses = [HL, C]; }
def LD_HLind_D : Inst8<"ld (hl),d", 0x72> { let Uses = [HL, D]; }
def LD_HLind_E : Inst8<"ld (hl),e", 0x73> { let Uses = [HL, E]; }
def LD_HLind_H : Inst8<"ld (hl),h", 0x74> { let Uses = [HL, H]; }
def LD_HLind_L : Inst8<"ld (hl),l", 0x75> { let Uses = [HL, L]; }
def LD_HLind_A : Inst8<"ld (hl),a", 0x77> { let Uses = [HL, A]; }
}

// LD (HL),n - Store immediate to (HL)
let mayLoad = false, mayStore = true, hasSideEffects = false in
def LD_HLind_n : Inst16<"ld (hl),", 0x36, Imm8Mode> { let Uses = [HL]; }

// LD A,(BC) / LD A,(DE) - Load from indirect
let mayLoad = true, mayStore = false, hasSideEffects = false in {
def LD_A_BCind : Inst8<"ld a,(bc)", 0x0A> { let Uses = [BC]; let Defs = [A]; }
def LD_A_DEind : Inst8<"ld a,(de)", 0x1A> { let Uses = [DE]; let Defs = [A]; }
}

// LD (BC/DE),A - Store to indirect
let mayLoad = false, mayStore = true, hasSideEffects = false in {
def LD_BCind_A : Inst8<"ld (bc),a", 0x02> { let Uses = [BC, A]; }
def LD_DEind_A : Inst8<"ld (de),a", 0x12> { let Uses = [DE, A]; }
}

//===----------------------------------------------------------------------===//
//  16-bit Load Group (common subset)
//===----------------------------------------------------------------------===//

// LD dd,nn - Load 16-bit immediate (defines the target register)
def LD_BC_nn : LDrrnn<REGPAIR_BC> { let Defs = [BC]; }
def LD_DE_nn : LDrrnn<REGPAIR_DE> { let Defs = [DE]; }
def LD_HL_nn : LDrrnn<REGPAIR_HL> { let Defs = [HL]; }
def LD_SP_nn : LDrrnn<REGPAIR_SP> { let Defs = [SP]; }

// LD SP,HL
def LD_SP_HL : Inst8<"ld sp,hl", 0xF9> { let Uses = [HL]; let Defs = [SP]; }

// PUSH/POP - stack memory access, no unmodeled side effects.
// Note: SP is not declared in Defs/Uses because LLVM's PEI handles
// SP adjustments separately. Peephole passes must check PUSH/POP
// opcodes explicitly when tracking SP.
let mayLoad = false, mayStore = true, hasSideEffects = false in {
def PUSH_BC : PUSHrr<REGPAIR_BC> { let Uses = [BC]; }
def PUSH_DE : PUSHrr<REGPAIR_DE> { let Uses = [DE]; }
def PUSH_HL : PUSHrr<REGPAIR_HL> { let Uses = [HL]; }
def PUSH_AF : PUSHrr<REGPAIR_AF> { let Uses = [A, FLAGS]; }
}

let mayLoad = true, mayStore = false, hasSideEffects = false in {
def POP_BC : POPrr<REGPAIR_BC> { let Defs = [BC]; }
def POP_DE : POPrr<REGPAIR_DE> { let Defs = [DE]; }
def POP_HL : POPrr<REGPAIR_HL> { let Defs = [HL]; }
def POP_AF : POPrr<REGPAIR_AF> { let Defs = [A, FLAGS]; }
}

//===----------------------------------------------------------------------===//
//  8-bit Arithmetic Group
//===----------------------------------------------------------------------===//

// ADD A,r - Add to A
def ADD_A_B : Inst8<"add a,b", 0x80> { let Uses = [A, B]; let Defs = [A, FLAGS]; }
def ADD_A_C : Inst8<"add a,c", 0x81> { let Uses = [A, C]; let Defs = [A, FLAGS]; }
def ADD_A_D : Inst8<"add a,d", 0x82> { let Uses = [A, D]; let Defs = [A, FLAGS]; }
def ADD_A_E : Inst8<"add a,e", 0x83> { let Uses = [A, E]; let Defs = [A, FLAGS]; }
def ADD_A_H : Inst8<"add a,h", 0x84> { let Uses = [A, H]; let Defs = [A, FLAGS]; }
def ADD_A_L : Inst8<"add a,l", 0x85> { let Uses = [A, L]; let Defs = [A, FLAGS]; }
let mayLoad = true, mayStore = false, hasSideEffects = false in
def ADD_A_HLind : Inst8<"add a,(hl)", 0x86> { let Uses = [A, HL]; let Defs = [A, FLAGS]; }
def ADD_A_A : Inst8<"add a,a", 0x87> { let Uses = [A]; let Defs = [A, FLAGS]; }
def ADD_A_n : Inst16<"add a,", 0xC6, Imm8Mode> { let Uses = [A]; let Defs = [A, FLAGS]; }

// ADC A,r - Add with carry to A (reads carry flag)
def ADC_A_B : Inst8<"adc a,b", 0x88> { let Uses = [A, B, FLAGS]; let Defs = [A, FLAGS]; }
def ADC_A_C : Inst8<"adc a,c", 0x89> { let Uses = [A, C, FLAGS]; let Defs = [A, FLAGS]; }
def ADC_A_D : Inst8<"adc a,d", 0x8A> { let Uses = [A, D, FLAGS]; let Defs = [A, FLAGS]; }
def ADC_A_E : Inst8<"adc a,e", 0x8B> { let Uses = [A, E, FLAGS]; let Defs = [A, FLAGS]; }
def ADC_A_H : Inst8<"adc a,h", 0x8C> { let Uses = [A, H, FLAGS]; let Defs = [A, FLAGS]; }
def ADC_A_L : Inst8<"adc a,l", 0x8D> { let Uses = [A, L, FLAGS]; let Defs = [A, FLAGS]; }
let mayLoad = true, mayStore = false, hasSideEffects = false in
def ADC_A_HLind : Inst8<"adc a,(hl)", 0x8E> { let Uses = [A, HL, FLAGS]; let Defs = [A, FLAGS]; }
def ADC_A_A : Inst8<"adc a,a", 0x8F> { let Uses = [A, FLAGS]; let Defs = [A, FLAGS]; }
def ADC_A_n : Inst16<"adc a,", 0xCE, Imm8Mode> { let Uses = [A, FLAGS]; let Defs = [A, FLAGS]; }

// SUB r - All subtract from A and store in A
def SUB_B : Inst8<"sub b", 0x90> { let Uses = [A, B]; let Defs = [A, FLAGS]; }
def SUB_C : Inst8<"sub c", 0x91> { let Uses = [A, C]; let Defs = [A, FLAGS]; }
def SUB_D : Inst8<"sub d", 0x92> { let Uses = [A, D]; let Defs = [A, FLAGS]; }
def SUB_E : Inst8<"sub e", 0x93> { let Uses = [A, E]; let Defs = [A, FLAGS]; }
def SUB_H : Inst8<"sub h", 0x94> { let Uses = [A, H]; let Defs = [A, FLAGS]; }
def SUB_L : Inst8<"sub l", 0x95> { let Uses = [A, L]; let Defs = [A, FLAGS]; }
let mayLoad = true, mayStore = false, hasSideEffects = false in
def SUB_HLind : Inst8<"sub (hl)", 0x96> { let Uses = [A, HL]; let Defs = [A, FLAGS]; }
def SUB_A : Inst8<"sub a", 0x97> { let Uses = [A]; let Defs = [A, FLAGS]; }
def SUB_n : Inst16<"sub", 0xD6, Imm8Mode> { let Uses = [A]; let Defs = [A, FLAGS]; }

// SBC A,r - Subtract with carry from A (reads carry flag)
def SBC_A_B : Inst8<"sbc a,b", 0x98> { let Uses = [A, B, FLAGS]; let Defs = [A, FLAGS]; }
def SBC_A_C : Inst8<"sbc a,c", 0x99> { let Uses = [A, C, FLAGS]; let Defs = [A, FLAGS]; }
def SBC_A_D : Inst8<"sbc a,d", 0x9A> { let Uses = [A, D, FLAGS]; let Defs = [A, FLAGS]; }
def SBC_A_E : Inst8<"sbc a,e", 0x9B> { let Uses = [A, E, FLAGS]; let Defs = [A, FLAGS]; }
def SBC_A_H : Inst8<"sbc a,h", 0x9C> { let Uses = [A, H, FLAGS]; let Defs = [A, FLAGS]; }
def SBC_A_L : Inst8<"sbc a,l", 0x9D> { let Uses = [A, L, FLAGS]; let Defs = [A, FLAGS]; }
let mayLoad = true, mayStore = false, hasSideEffects = false in
def SBC_A_HLind : Inst8<"sbc a,(hl)", 0x9E> { let Uses = [A, HL, FLAGS]; let Defs = [A, FLAGS]; }
def SBC_A_A : Inst8<"sbc a,a", 0x9F> { let Uses = [A, FLAGS]; let Defs = [A, FLAGS]; }
def SBC_A_n : Inst16<"sbc a,", 0xDE, Imm8Mode> { let Uses = [A, FLAGS]; let Defs = [A, FLAGS]; }

// AND r - Logical AND with A
def AND_B : Inst8<"and b", 0xA0> { let Uses = [A, B]; let Defs = [A, FLAGS]; }
def AND_C : Inst8<"and c", 0xA1> { let Uses = [A, C]; let Defs = [A, FLAGS]; }
def AND_D : Inst8<"and d", 0xA2> { let Uses = [A, D]; let Defs = [A, FLAGS]; }
def AND_E : Inst8<"and e", 0xA3> { let Uses = [A, E]; let Defs = [A, FLAGS]; }
def AND_H : Inst8<"and h", 0xA4> { let Uses = [A, H]; let Defs = [A, FLAGS]; }
def AND_L : Inst8<"and l", 0xA5> { let Uses = [A, L]; let Defs = [A, FLAGS]; }
let mayLoad = true, mayStore = false, hasSideEffects = false in
def AND_HLind : Inst8<"and (hl)", 0xA6> { let Uses = [A, HL]; let Defs = [A, FLAGS]; }
def AND_A : Inst8<"and a", 0xA7> { let Uses = [A]; let Defs = [A, FLAGS]; }
def AND_n : Inst16<"and", 0xE6, Imm8Mode> { let Uses = [A]; let Defs = [A, FLAGS]; }

// XOR r - Logical XOR with A
def XOR_B : Inst8<"xor b", 0xA8> { let Uses = [A, B]; let Defs = [A, FLAGS]; }
def XOR_C : Inst8<"xor c", 0xA9> { let Uses = [A, C]; let Defs = [A, FLAGS]; }
def XOR_D : Inst8<"xor d", 0xAA> { let Uses = [A, D]; let Defs = [A, FLAGS]; }
def XOR_E : Inst8<"xor e", 0xAB> { let Uses = [A, E]; let Defs = [A, FLAGS]; }
def XOR_H : Inst8<"xor h", 0xAC> { let Uses = [A, H]; let Defs = [A, FLAGS]; }
def XOR_L : Inst8<"xor l", 0xAD> { let Uses = [A, L]; let Defs = [A, FLAGS]; }
let mayLoad = true, mayStore = false, hasSideEffects = false in
def XOR_HLind : Inst8<"xor (hl)", 0xAE> { let Uses = [A, HL]; let Defs = [A, FLAGS]; }
def XOR_A : Inst8<"xor a", 0xAF> { let Uses = [A]; let Defs = [A, FLAGS]; }
def XOR_n : Inst16<"xor", 0xEE, Imm8Mode> { let Uses = [A]; let Defs = [A, FLAGS]; }

// OR r - Logical OR with A
def OR_B : Inst8<"or b", 0xB0> { let Uses = [A, B]; let Defs = [A, FLAGS]; }
def OR_C : Inst8<"or c", 0xB1> { let Uses = [A, C]; let Defs = [A, FLAGS]; }
def OR_D : Inst8<"or d", 0xB2> { let Uses = [A, D]; let Defs = [A, FLAGS]; }
def OR_E : Inst8<"or e", 0xB3> { let Uses = [A, E]; let Defs = [A, FLAGS]; }
def OR_H : Inst8<"or h", 0xB4> { let Uses = [A, H]; let Defs = [A, FLAGS]; }
def OR_L : Inst8<"or l", 0xB5> { let Uses = [A, L]; let Defs = [A, FLAGS]; }
let mayLoad = true, mayStore = false, hasSideEffects = false in
def OR_HLind : Inst8<"or (hl)", 0xB6> { let Uses = [A, HL]; let Defs = [A, FLAGS]; }
def OR_A : Inst8<"or a", 0xB7> { let Uses = [A]; let Defs = [A, FLAGS]; }
def OR_n : Inst16<"or", 0xF6, Imm8Mode> { let Uses = [A]; let Defs = [A, FLAGS]; }

// CP r - Compare with A (only sets flags, doesn't modify A)
def CP_B : Inst8<"cp b", 0xB8> { let Uses = [A, B]; let Defs = [FLAGS]; }
def CP_C : Inst8<"cp c", 0xB9> { let Uses = [A, C]; let Defs = [FLAGS]; }
def CP_D : Inst8<"cp d", 0xBA> { let Uses = [A, D]; let Defs = [FLAGS]; }
def CP_E : Inst8<"cp e", 0xBB> { let Uses = [A, E]; let Defs = [FLAGS]; }
def CP_H : Inst8<"cp h", 0xBC> { let Uses = [A, H]; let Defs = [FLAGS]; }
def CP_L : Inst8<"cp l", 0xBD> { let Uses = [A, L]; let Defs = [FLAGS]; }
let mayLoad = true, mayStore = false, hasSideEffects = false in
def CP_HLind : Inst8<"cp (hl)", 0xBE> { let Uses = [A, HL]; let Defs = [FLAGS]; }
def CP_A : Inst8<"cp a", 0xBF> { let Uses = [A]; let Defs = [FLAGS]; }
def CP_n : Inst16<"cp", 0xFE, Imm8Mode> { let Uses = [A]; let Defs = [FLAGS]; }

// INC/DEC r - 8-bit INC/DEC set all flags except carry
def INC_B : INCr<REG_B> { let Uses = [B]; let Defs = [B, FLAGS]; }
def INC_C : INCr<REG_C> { let Uses = [C]; let Defs = [C, FLAGS]; }
def INC_D : INCr<REG_D> { let Uses = [D]; let Defs = [D, FLAGS]; }
def INC_E : INCr<REG_E> { let Uses = [E]; let Defs = [E, FLAGS]; }
def INC_H : INCr<REG_H> { let Uses = [H]; let Defs = [H, FLAGS]; }
def INC_L : INCr<REG_L> { let Uses = [L]; let Defs = [L, FLAGS]; }
let mayLoad = true, mayStore = true, hasSideEffects = false in
def INC_HLind : Inst8<"inc (hl)", 0x34> { let Uses = [HL]; let Defs = [FLAGS]; }
def INC_A : Inst8<"inc a", 0x3C> { let Uses = [A]; let Defs = [A, FLAGS]; }

def DEC_B : DECr<REG_B> { let Uses = [B]; let Defs = [B, FLAGS]; }
def DEC_C : DECr<REG_C> { let Uses = [C]; let Defs = [C, FLAGS]; }
def DEC_D : DECr<REG_D> { let Uses = [D]; let Defs = [D, FLAGS]; }
def DEC_E : DECr<REG_E> { let Uses = [E]; let Defs = [E, FLAGS]; }
def DEC_H : DECr<REG_H> { let Uses = [H]; let Defs = [H, FLAGS]; }
def DEC_L : DECr<REG_L> { let Uses = [L]; let Defs = [L, FLAGS]; }
let mayLoad = true, mayStore = true, hasSideEffects = false in
def DEC_HLind : Inst8<"dec (hl)", 0x35> { let Uses = [HL]; let Defs = [FLAGS]; }
def DEC_A : Inst8<"dec a", 0x3D> { let Uses = [A]; let Defs = [A, FLAGS]; }

//===----------------------------------------------------------------------===//
//  General-Purpose Arithmetic and CPU Control (common subset)
//===----------------------------------------------------------------------===//

def DAA : Inst8<"daa", 0x27> { let Uses = [A, FLAGS]; let Defs = [A, FLAGS]; }
def CPL : Inst8<"cpl", 0x2F> { let Uses = [A]; let Defs = [A, FLAGS]; }
def CCF : Inst8<"ccf", 0x3F> { let Uses = [FLAGS]; let Defs = [FLAGS]; }
def SCF : Inst8<"scf", 0x37> { let Defs = [FLAGS]; }
def NOP : Inst8<"nop", 0x00>;
def HALT : Inst8<"halt", 0x76>;
def DI : Inst8<"di", 0xF3>;
def EI : Inst8<"ei", 0xFB>;

//===----------------------------------------------------------------------===//
//  16-bit Arithmetic Group (common subset)
//===----------------------------------------------------------------------===//

// ADD HL,ss - Uses HL and the source register pair, defines HL (sets carry, H)
def ADD_HL_BC : ADDHLrr<REGPAIR_BC> { let Uses = [HL, BC]; let Defs = [HL, FLAGS]; }
def ADD_HL_DE : ADDHLrr<REGPAIR_DE> { let Uses = [HL, DE]; let Defs = [HL, FLAGS]; }
def ADD_HL_HL : ADDHLrr<REGPAIR_HL> { let Uses = [HL]; let Defs = [HL, FLAGS]; }
def ADD_HL_SP : ADDHLrr<REGPAIR_SP> { let Uses = [HL, SP]; let Defs = [HL, FLAGS]; }

// INC/DEC ss
def INC_BC : INCrr<REGPAIR_BC> { let Uses = [BC]; let Defs = [BC]; }
def INC_DE : INCrr<REGPAIR_DE> { let Uses = [DE]; let Defs = [DE]; }
def INC_HL : INCrr<REGPAIR_HL> { let Uses = [HL]; let Defs = [HL]; }
def INC_SP : INCrr<REGPAIR_SP> { let Uses = [SP]; let Defs = [SP]; }

def DEC_BC : DECrr<REGPAIR_BC> { let Uses = [BC]; let Defs = [BC]; }
def DEC_DE : DECrr<REGPAIR_DE> { let Uses = [DE]; let Defs = [DE]; }
def DEC_HL : DECrr<REGPAIR_HL> { let Uses = [HL]; let Defs = [HL]; }
def DEC_SP : DECrr<REGPAIR_SP> { let Uses = [SP]; let Defs = [SP]; }

//===----------------------------------------------------------------------===//
//  Rotate and Shift Group
//===----------------------------------------------------------------------===//

// Accumulator rotates — read and modify A
// RLCA/RRCA: don't read carry. RLA/RRA: read carry (rotate through carry).
def RLCA : Inst8<"rlca", 0x07> { let Uses = [A]; let Defs = [A, FLAGS]; }
def RRCA : Inst8<"rrca", 0x0F> { let Uses = [A]; let Defs = [A, FLAGS]; }
def RLA  : Inst8<"rla",  0x17> { let Uses = [A, FLAGS]; let Defs = [A, FLAGS]; }
def RRA  : Inst8<"rra",  0x1F> { let Uses = [A, FLAGS]; let Defs = [A, FLAGS]; }

// CB-prefixed rotates/shifts
// RLC r (don't read carry)
def RLC_B : CBRotate<0b000, REG_B, "rlc"> { let Defs = [B, FLAGS]; let Uses = [B]; }
def RLC_C : CBRotate<0b000, REG_C, "rlc"> { let Defs = [C, FLAGS]; let Uses = [C]; }
def RLC_D : CBRotate<0b000, REG_D, "rlc"> { let Defs = [D, FLAGS]; let Uses = [D]; }
def RLC_E : CBRotate<0b000, REG_E, "rlc"> { let Defs = [E, FLAGS]; let Uses = [E]; }
def RLC_H : CBRotate<0b000, REG_H, "rlc"> { let Defs = [H, FLAGS]; let Uses = [H]; }
def RLC_L : CBRotate<0b000, REG_L, "rlc"> { let Defs = [L, FLAGS]; let Uses = [L]; }
let mayLoad = true, mayStore = true, hasSideEffects = false in
def RLC_HLind : InstCB<"rlc (hl)", 0x06> { let Uses = [HL]; let Defs = [FLAGS]; }
def RLC_A : CBRotate<0b000, REG_A, "rlc"> { let Defs = [A, FLAGS]; let Uses = [A]; }

// RRC r (don't read carry)
def RRC_B : CBRotate<0b001, REG_B, "rrc"> { let Defs = [B, FLAGS]; let Uses = [B]; }
def RRC_C : CBRotate<0b001, REG_C, "rrc"> { let Defs = [C, FLAGS]; let Uses = [C]; }
def RRC_D : CBRotate<0b001, REG_D, "rrc"> { let Defs = [D, FLAGS]; let Uses = [D]; }
def RRC_E : CBRotate<0b001, REG_E, "rrc"> { let Defs = [E, FLAGS]; let Uses = [E]; }
def RRC_H : CBRotate<0b001, REG_H, "rrc"> { let Defs = [H, FLAGS]; let Uses = [H]; }
def RRC_L : CBRotate<0b001, REG_L, "rrc"> { let Defs = [L, FLAGS]; let Uses = [L]; }
let mayLoad = true, mayStore = true, hasSideEffects = false in
def RRC_HLind : InstCB<"rrc (hl)", 0x0E> { let Uses = [HL]; let Defs = [FLAGS]; }
def RRC_A : CBRotate<0b001, REG_A, "rrc"> { let Defs = [A, FLAGS]; let Uses = [A]; }

// RL r (reads carry, rotate through carry)
def RL_B : CBRotate<0b010, REG_B, "rl"> { let Defs = [B, FLAGS]; let Uses = [B, FLAGS]; }
def RL_C : CBRotate<0b010, REG_C, "rl"> { let Defs = [C, FLAGS]; let Uses = [C, FLAGS]; }
def RL_D : CBRotate<0b010, REG_D, "rl"> { let Defs = [D, FLAGS]; let Uses = [D, FLAGS]; }
def RL_E : CBRotate<0b010, REG_E, "rl"> { let Defs = [E, FLAGS]; let Uses = [E, FLAGS]; }
def RL_H : CBRotate<0b010, REG_H, "rl"> { let Defs = [H, FLAGS]; let Uses = [H, FLAGS]; }
def RL_L : CBRotate<0b010, REG_L, "rl"> { let Defs = [L, FLAGS]; let Uses = [L, FLAGS]; }
let mayLoad = true, mayStore = true, hasSideEffects = false in
def RL_HLind : InstCB<"rl (hl)", 0x16> { let Uses = [HL, FLAGS]; let Defs = [FLAGS]; }
def RL_A : CBRotate<0b010, REG_A, "rl"> { let Defs = [A, FLAGS]; let Uses = [A, FLAGS]; }

// RR r (reads carry, rotate through carry)
def RR_B : CBRotate<0b011, REG_B, "rr"> { let Defs = [B, FLAGS]; let Uses = [B, FLAGS]; }
def RR_C : CBRotate<0b011, REG_C, "rr"> { let Defs = [C, FLAGS]; let Uses = [C, FLAGS]; }
def RR_D : CBRotate<0b011, REG_D, "rr"> { let Defs = [D, FLAGS]; let Uses = [D, FLAGS]; }
def RR_E : CBRotate<0b011, REG_E, "rr"> { let Defs = [E, FLAGS]; let Uses = [E, FLAGS]; }
def RR_H : CBRotate<0b011, REG_H, "rr"> { let Defs = [H, FLAGS]; let Uses = [H, FLAGS]; }
def RR_L : CBRotate<0b011, REG_L, "rr"> { let Defs = [L, FLAGS]; let Uses = [L, FLAGS]; }
let mayLoad = true, mayStore = true, hasSideEffects = false in
def RR_HLind : InstCB<"rr (hl)", 0x1E> { let Uses = [HL, FLAGS]; let Defs = [FLAGS]; }
def RR_A : CBRotate<0b011, REG_A, "rr"> { let Defs = [A, FLAGS]; let Uses = [A, FLAGS]; }

// SLA r (don't read carry)
def SLA_B : CBRotate<0b100, REG_B, "sla"> { let Defs = [B, FLAGS]; let Uses = [B]; }
def SLA_C : CBRotate<0b100, REG_C, "sla"> { let Defs = [C, FLAGS]; let Uses = [C]; }
def SLA_D : CBRotate<0b100, REG_D, "sla"> { let Defs = [D, FLAGS]; let Uses = [D]; }
def SLA_E : CBRotate<0b100, REG_E, "sla"> { let Defs = [E, FLAGS]; let Uses = [E]; }
def SLA_H : CBRotate<0b100, REG_H, "sla"> { let Defs = [H, FLAGS]; let Uses = [H]; }
def SLA_L : CBRotate<0b100, REG_L, "sla"> { let Defs = [L, FLAGS]; let Uses = [L]; }
let mayLoad = true, mayStore = true, hasSideEffects = false in
def SLA_HLind : InstCB<"sla (hl)", 0x26> { let Uses = [HL]; let Defs = [FLAGS]; }
def SLA_A : CBRotate<0b100, REG_A, "sla"> { let Defs = [A, FLAGS]; let Uses = [A]; }

// SRA r (don't read carry)
def SRA_B : CBRotate<0b101, REG_B, "sra"> { let Defs = [B, FLAGS]; let Uses = [B]; }
def SRA_C : CBRotate<0b101, REG_C, "sra"> { let Defs = [C, FLAGS]; let Uses = [C]; }
def SRA_D : CBRotate<0b101, REG_D, "sra"> { let Defs = [D, FLAGS]; let Uses = [D]; }
def SRA_E : CBRotate<0b101, REG_E, "sra"> { let Defs = [E, FLAGS]; let Uses = [E]; }
def SRA_H : CBRotate<0b101, REG_H, "sra"> { let Defs = [H, FLAGS]; let Uses = [H]; }
def SRA_L : CBRotate<0b101, REG_L, "sra"> { let Defs = [L, FLAGS]; let Uses = [L]; }
let mayLoad = true, mayStore = true, hasSideEffects = false in
def SRA_HLind : InstCB<"sra (hl)", 0x2E> { let Uses = [HL]; let Defs = [FLAGS]; }
def SRA_A : CBRotate<0b101, REG_A, "sra"> { let Defs = [A, FLAGS]; let Uses = [A]; }

// SRL r (don't read carry)
def SRL_B : CBRotate<0b111, REG_B, "srl"> { let Defs = [B, FLAGS]; let Uses = [B]; }
def SRL_C : CBRotate<0b111, REG_C, "srl"> { let Defs = [C, FLAGS]; let Uses = [C]; }
def SRL_D : CBRotate<0b111, REG_D, "srl"> { let Defs = [D, FLAGS]; let Uses = [D]; }
def SRL_E : CBRotate<0b111, REG_E, "srl"> { let Defs = [E, FLAGS]; let Uses = [E]; }
def SRL_H : CBRotate<0b111, REG_H, "srl"> { let Defs = [H, FLAGS]; let Uses = [H]; }
def SRL_L : CBRotate<0b111, REG_L, "srl"> { let Defs = [L, FLAGS]; let Uses = [L]; }
let mayLoad = true, mayStore = true, hasSideEffects = false in
def SRL_HLind : InstCB<"srl (hl)", 0x3E> { let Uses = [HL]; let Defs = [FLAGS]; }
def SRL_A : CBRotate<0b111, REG_A, "srl"> { let Defs = [A, FLAGS]; let Uses = [A]; }

//===----------------------------------------------------------------------===//
//  Bit Set, Reset, and Test Group (CB prefixed)
//===----------------------------------------------------------------------===//

// BIT b,r - Test bit (sets Z flag)
let Defs = [FLAGS] in {
def BIT_0_B : BITbr<0, REG_B>;  def BIT_0_C : BITbr<0, REG_C>;
def BIT_0_D : BITbr<0, REG_D>;  def BIT_0_E : BITbr<0, REG_E>;
def BIT_0_H : BITbr<0, REG_H>;  def BIT_0_L : BITbr<0, REG_L>;
def BIT_0_A : BITbr<0, REG_A>;
let mayLoad = true, mayStore = false, hasSideEffects = false in
def BIT_0_HLind : InstCB<"bit 0,(hl)", 0x46>;

def BIT_1_B : BITbr<1, REG_B>;  def BIT_1_C : BITbr<1, REG_C>;
def BIT_1_D : BITbr<1, REG_D>;  def BIT_1_E : BITbr<1, REG_E>;
def BIT_1_H : BITbr<1, REG_H>;  def BIT_1_L : BITbr<1, REG_L>;
def BIT_1_A : BITbr<1, REG_A>;
let mayLoad = true, mayStore = false, hasSideEffects = false in
def BIT_1_HLind : InstCB<"bit 1,(hl)", 0x4E>;

def BIT_2_B : BITbr<2, REG_B>;  def BIT_2_C : BITbr<2, REG_C>;
def BIT_2_D : BITbr<2, REG_D>;  def BIT_2_E : BITbr<2, REG_E>;
def BIT_2_H : BITbr<2, REG_H>;  def BIT_2_L : BITbr<2, REG_L>;
def BIT_2_A : BITbr<2, REG_A>;
let mayLoad = true, mayStore = false, hasSideEffects = false in
def BIT_2_HLind : InstCB<"bit 2,(hl)", 0x56>;

def BIT_3_B : BITbr<3, REG_B>;  def BIT_3_C : BITbr<3, REG_C>;
def BIT_3_D : BITbr<3, REG_D>;  def BIT_3_E : BITbr<3, REG_E>;
def BIT_3_H : BITbr<3, REG_H>;  def BIT_3_L : BITbr<3, REG_L>;
def BIT_3_A : BITbr<3, REG_A>;
let mayLoad = true, mayStore = false, hasSideEffects = false in
def BIT_3_HLind : InstCB<"bit 3,(hl)", 0x5E>;

def BIT_4_B : BITbr<4, REG_B>;  def BIT_4_C : BITbr<4, REG_C>;
def BIT_4_D : BITbr<4, REG_D>;  def BIT_4_E : BITbr<4, REG_E>;
def BIT_4_H : BITbr<4, REG_H>;  def BIT_4_L : BITbr<4, REG_L>;
def BIT_4_A : BITbr<4, REG_A>;
let mayLoad = true, mayStore = false, hasSideEffects = false in
def BIT_4_HLind : InstCB<"bit 4,(hl)", 0x66>;

def BIT_5_B : BITbr<5, REG_B>;  def BIT_5_C : BITbr<5, REG_C>;
def BIT_5_D : BITbr<5, REG_D>;  def BIT_5_E : BITbr<5, REG_E>;
def BIT_5_H : BITbr<5, REG_H>;  def BIT_5_L : BITbr<5, REG_L>;
def BIT_5_A : BITbr<5, REG_A>;
let mayLoad = true, mayStore = false, hasSideEffects = false in
def BIT_5_HLind : InstCB<"bit 5,(hl)", 0x6E>;

def BIT_6_B : BITbr<6, REG_B>;  def BIT_6_C : BITbr<6, REG_C>;
def BIT_6_D : BITbr<6, REG_D>;  def BIT_6_E : BITbr<6, REG_E>;
def BIT_6_H : BITbr<6, REG_H>;  def BIT_6_L : BITbr<6, REG_L>;
def BIT_6_A : BITbr<6, REG_A>;
let mayLoad = true, mayStore = false, hasSideEffects = false in
def BIT_6_HLind : InstCB<"bit 6,(hl)", 0x76>;

def BIT_7_B : BITbr<7, REG_B>;  def BIT_7_C : BITbr<7, REG_C>;
def BIT_7_D : BITbr<7, REG_D>;  def BIT_7_E : BITbr<7, REG_E>;
def BIT_7_H : BITbr<7, REG_H>;  def BIT_7_L : BITbr<7, REG_L>;
def BIT_7_A : BITbr<7, REG_A>;
let mayLoad = true, mayStore = false, hasSideEffects = false in
def BIT_7_HLind : InstCB<"bit 7,(hl)", 0x7E>;
} // Defs = [FLAGS]

// RES b,r - Reset bit
def RES_0_B : RESbr<0, REG_B>;  def RES_0_C : RESbr<0, REG_C>;
def RES_0_D : RESbr<0, REG_D>;  def RES_0_E : RESbr<0, REG_E>;
def RES_0_H : RESbr<0, REG_H>;  def RES_0_L : RESbr<0, REG_L>;
def RES_0_A : RESbr<0, REG_A>;

def RES_1_B : RESbr<1, REG_B>;  def RES_1_C : RESbr<1, REG_C>;
def RES_1_D : RESbr<1, REG_D>;  def RES_1_E : RESbr<1, REG_E>;
def RES_1_H : RESbr<1, REG_H>;  def RES_1_L : RESbr<1, REG_L>;
def RES_1_A : RESbr<1, REG_A>;

def RES_2_B : RESbr<2, REG_B>;  def RES_2_C : RESbr<2, REG_C>;
def RES_2_D : RESbr<2, REG_D>;  def RES_2_E : RESbr<2, REG_E>;
def RES_2_H : RESbr<2, REG_H>;  def RES_2_L : RESbr<2, REG_L>;
def RES_2_A : RESbr<2, REG_A>;

def RES_3_B : RESbr<3, REG_B>;  def RES_3_C : RESbr<3, REG_C>;
def RES_3_D : RESbr<3, REG_D>;  def RES_3_E : RESbr<3, REG_E>;
def RES_3_H : RESbr<3, REG_H>;  def RES_3_L : RESbr<3, REG_L>;
def RES_3_A : RESbr<3, REG_A>;

def RES_4_B : RESbr<4, REG_B>;  def RES_4_C : RESbr<4, REG_C>;
def RES_4_D : RESbr<4, REG_D>;  def RES_4_E : RESbr<4, REG_E>;
def RES_4_H : RESbr<4, REG_H>;  def RES_4_L : RESbr<4, REG_L>;
def RES_4_A : RESbr<4, REG_A>;

def RES_5_B : RESbr<5, REG_B>;  def RES_5_C : RESbr<5, REG_C>;
def RES_5_D : RESbr<5, REG_D>;  def RES_5_E : RESbr<5, REG_E>;
def RES_5_H : RESbr<5, REG_H>;  def RES_5_L : RESbr<5, REG_L>;
def RES_5_A : RESbr<5, REG_A>;

def RES_6_B : RESbr<6, REG_B>;  def RES_6_C : RESbr<6, REG_C>;
def RES_6_D : RESbr<6, REG_D>;  def RES_6_E : RESbr<6, REG_E>;
def RES_6_H : RESbr<6, REG_H>;  def RES_6_L : RESbr<6, REG_L>;
def RES_6_A : RESbr<6, REG_A>;

def RES_7_B : RESbr<7, REG_B>;  def RES_7_C : RESbr<7, REG_C>;
def RES_7_D : RESbr<7, REG_D>;  def RES_7_E : RESbr<7, REG_E>;
def RES_7_H : RESbr<7, REG_H>;  def RES_7_L : RESbr<7, REG_L>;
def RES_7_A : RESbr<7, REG_A>;

// RES b,(HL) - Reset bit in memory
let mayLoad = true, mayStore = true, hasSideEffects = false in {
def RES_0_HLind : InstCB<"res 0,(hl)", 0x86> { let Uses = [HL]; }
def RES_1_HLind : InstCB<"res 1,(hl)", 0x8E> { let Uses = [HL]; }
def RES_2_HLind : InstCB<"res 2,(hl)", 0x96> { let Uses = [HL]; }
def RES_3_HLind : InstCB<"res 3,(hl)", 0x9E> { let Uses = [HL]; }
def RES_4_HLind : InstCB<"res 4,(hl)", 0xA6> { let Uses = [HL]; }
def RES_5_HLind : InstCB<"res 5,(hl)", 0xAE> { let Uses = [HL]; }
def RES_6_HLind : InstCB<"res 6,(hl)", 0xB6> { let Uses = [HL]; }
def RES_7_HLind : InstCB<"res 7,(hl)", 0xBE> { let Uses = [HL]; }
}

// SET b,r - Set bit
def SET_0_B : SETbr<0, REG_B>;  def SET_0_C : SETbr<0, REG_C>;
def SET_0_D : SETbr<0, REG_D>;  def SET_0_E : SETbr<0, REG_E>;
def SET_0_H : SETbr<0, REG_H>;  def SET_0_L : SETbr<0, REG_L>;
def SET_0_A : SETbr<0, REG_A>;

def SET_1_B : SETbr<1, REG_B>;  def SET_1_C : SETbr<1, REG_C>;
def SET_1_D : SETbr<1, REG_D>;  def SET_1_E : SETbr<1, REG_E>;
def SET_1_H : SETbr<1, REG_H>;  def SET_1_L : SETbr<1, REG_L>;
def SET_1_A : SETbr<1, REG_A>;

def SET_2_B : SETbr<2, REG_B>;  def SET_2_C : SETbr<2, REG_C>;
def SET_2_D : SETbr<2, REG_D>;  def SET_2_E : SETbr<2, REG_E>;
def SET_2_H : SETbr<2, REG_H>;  def SET_2_L : SETbr<2, REG_L>;
def SET_2_A : SETbr<2, REG_A>;

def SET_3_B : SETbr<3, REG_B>;  def SET_3_C : SETbr<3, REG_C>;
def SET_3_D : SETbr<3, REG_D>;  def SET_3_E : SETbr<3, REG_E>;
def SET_3_H : SETbr<3, REG_H>;  def SET_3_L : SETbr<3, REG_L>;
def SET_3_A : SETbr<3, REG_A>;

def SET_4_B : SETbr<4, REG_B>;  def SET_4_C : SETbr<4, REG_C>;
def SET_4_D : SETbr<4, REG_D>;  def SET_4_E : SETbr<4, REG_E>;
def SET_4_H : SETbr<4, REG_H>;  def SET_4_L : SETbr<4, REG_L>;
def SET_4_A : SETbr<4, REG_A>;

def SET_5_B : SETbr<5, REG_B>;  def SET_5_C : SETbr<5, REG_C>;
def SET_5_D : SETbr<5, REG_D>;  def SET_5_E : SETbr<5, REG_E>;
def SET_5_H : SETbr<5, REG_H>;  def SET_5_L : SETbr<5, REG_L>;
def SET_5_A : SETbr<5, REG_A>;

def SET_6_B : SETbr<6, REG_B>;  def SET_6_C : SETbr<6, REG_C>;
def SET_6_D : SETbr<6, REG_D>;  def SET_6_E : SETbr<6, REG_E>;
def SET_6_H : SETbr<6, REG_H>;  def SET_6_L : SETbr<6, REG_L>;
def SET_6_A : SETbr<6, REG_A>;

def SET_7_B : SETbr<7, REG_B>;  def SET_7_C : SETbr<7, REG_C>;
def SET_7_D : SETbr<7, REG_D>;  def SET_7_E : SETbr<7, REG_E>;
def SET_7_H : SETbr<7, REG_H>;  def SET_7_L : SETbr<7, REG_L>;
def SET_7_A : SETbr<7, REG_A>;

// SET b,(HL) - Set bit in memory
let mayLoad = true, mayStore = true, hasSideEffects = false in {
def SET_0_HLind : InstCB<"set 0,(hl)", 0xC6> { let Uses = [HL]; }
def SET_1_HLind : InstCB<"set 1,(hl)", 0xCE> { let Uses = [HL]; }
def SET_2_HLind : InstCB<"set 2,(hl)", 0xD6> { let Uses = [HL]; }
def SET_3_HLind : InstCB<"set 3,(hl)", 0xDE> { let Uses = [HL]; }
def SET_4_HLind : InstCB<"set 4,(hl)", 0xE6> { let Uses = [HL]; }
def SET_5_HLind : InstCB<"set 5,(hl)", 0xEE> { let Uses = [HL]; }
def SET_6_HLind : InstCB<"set 6,(hl)", 0xF6> { let Uses = [HL]; }
def SET_7_HLind : InstCB<"set 7,(hl)", 0xFE> { let Uses = [HL]; }
}

//===----------------------------------------------------------------------===//
//  Jump Group (common subset: NZ/Z/NC/C conditions only)
//===----------------------------------------------------------------------===//

// JP nn - Unconditional jump
def JP_nn : Inst24<"jp", 0xC3, Imm16Mode>, InstUnconditionalBranch;

// JP cc,nn - Conditional jump (NZ/Z/NC/C only — shared with SM83)
def JP_NZ_nn : JPcond<CC_NZ>;
def JP_Z_nn  : JPcond<CC_Z>;
def JP_NC_nn : JPcond<CC_NC>;
def JP_C_nn  : JPcond<CC_C>;

// JR e - Relative jump
def JR_e : Inst16<"jr", 0x18, RelativeMode>, InstUnconditionalBranch;

// JR cc,e - Conditional relative jump (only NZ/Z/NC/C)
def JR_NZ_e : JRcond<CC_NZ>;
def JR_Z_e  : JRcond<CC_Z>;
def JR_NC_e : JRcond<CC_NC>;
def JR_C_e  : JRcond<CC_C>;

// JP (HL) - Jump to address in HL
def JP_HLind : Inst8<"jp (hl)", 0xE9>, InstUnconditionalBranch {
  let isIndirectBranch = true;
  let Uses = [HL];
}

//===----------------------------------------------------------------------===//
//  Call and Return Group (common subset: NZ/Z/NC/C conditions only)
//===----------------------------------------------------------------------===//

// CALL nn - Caller-saved registers are clobbered by calls.
// Argument registers are added dynamically as implicit uses in lowerCall.
// IY: caller-saved on Z80; harmless on SM83 where IY is reserved.
def CALL_nn : Inst24<"call", 0xCD, Imm16Mode>, InstCall {
  let Defs = [A, BC, DE, HL, IY, FLAGS];
}

// CALL cc,nn (NZ/Z/NC/C only — shared with SM83)
def CALL_NZ_nn : CALLcond<CC_NZ>;
def CALL_Z_nn  : CALLcond<CC_Z>;
def CALL_NC_nn : CALLcond<CC_NC>;
def CALL_C_nn  : CALLcond<CC_C>;

// RET - Return from subroutine
// Note: Return value registers (A, HL, DE) are added as implicit uses
// dynamically in lowerReturn based on the actual return type.
def RET : Inst8<"ret", 0xC9>, InstReturn;

// RET cc (NZ/Z/NC/C only — shared with SM83)
def RET_NZ : RETcond<CC_NZ>;
def RET_Z  : RETcond<CC_Z>;
def RET_NC : RETcond<CC_NC>;
def RET_C  : RETcond<CC_C>;

// RST p
def RST_00 : RSTvec<0>;
def RST_08 : RSTvec<1>;
def RST_10 : RSTvec<2>;
def RST_18 : RSTvec<3>;
def RST_20 : RSTvec<4>;
def RST_28 : RSTvec<5>;
def RST_30 : RSTvec<6>;
def RST_38 : RSTvec<7>;
