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
let mayLoad = 0, mayStore = 0, hasSideEffects = 0 in {
def LD_r_r : LDrr<"ld">;

// LD r,n - Load immediate to register
def LD_r_n : LDrn<"ld"> {
  let isAsCheapAsAMove = 1;
  let isMoveImm = 1;
  let isReMaterializable = 1;
}
}

// LD r,(HL) - Load from memory at HL
let mayLoad = true, mayStore = false, hasSideEffects = false in
def LD_r_HLind : LDrHLind<"ld">;

// LD (HL),r - Store to memory at HL
let mayLoad = false, mayStore = true, hasSideEffects = false in
def LD_HLind_r : LDHLindr<"ld">;

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
let mayLoad = 0, mayStore = 0, hasSideEffects = 0 in {
def LD_rr_nn : LDrrnn<"ld"> {
  let isAsCheapAsAMove = 1;
  let isMoveImm = 1;
  let isReMaterializable = 1;
}
}
def LD_SP_nn : Inst24<"ld sp,", 0x31, Imm16Mode> { let Defs = [SP]; }

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

// ALU A,r - The eight accumulator operations against a register.
let mayLoad = 0, mayStore = 0, hasSideEffects = 0 in {
def ADD_A_r : ALUAr<"add a,", 0b000> { let Uses = [A]; let Defs = [A, FLAGS]; }
def ADC_A_r : ALUAr<"adc a,", 0b001> { let Uses = [A, FLAGS]; let Defs = [A, FLAGS]; }
def SUB_r   : ALUAr<"sub",    0b010> { let Uses = [A]; let Defs = [A, FLAGS]; }
def SBC_A_r : ALUAr<"sbc a,", 0b011> { let Uses = [A, FLAGS]; let Defs = [A, FLAGS]; }
def AND_r   : ALUAr<"and",    0b100> { let Uses = [A]; let Defs = [A, FLAGS]; }
def XOR_r   : ALUAr<"xor",    0b101> { let Uses = [A]; let Defs = [A, FLAGS]; }
def OR_r    : ALUAr<"or",     0b110> { let Uses = [A]; let Defs = [A, FLAGS]; }
def CP_r    : ALUAr<"cp",     0b111> { let Uses = [A]; let Defs = [FLAGS]; }
}

// ALU A,(HL) - the same operations against the byte HL points at.
let mayLoad = true, mayStore = false, hasSideEffects = false in {
def ADD_A_HLind : Inst8<"add a,(hl)", 0x86> { let Uses = [A, HL]; let Defs = [A, FLAGS]; }
def ADC_A_HLind : Inst8<"adc a,(hl)", 0x8E> { let Uses = [A, HL, FLAGS]; let Defs = [A, FLAGS]; }
def SUB_HLind : Inst8<"sub (hl)", 0x96> { let Uses = [A, HL]; let Defs = [A, FLAGS]; }
def SBC_A_HLind : Inst8<"sbc a,(hl)", 0x9E> { let Uses = [A, HL, FLAGS]; let Defs = [A, FLAGS]; }
def AND_HLind : Inst8<"and (hl)", 0xA6> { let Uses = [A, HL]; let Defs = [A, FLAGS]; }
def XOR_HLind : Inst8<"xor (hl)", 0xAE> { let Uses = [A, HL]; let Defs = [A, FLAGS]; }
def OR_HLind : Inst8<"or (hl)", 0xB6> { let Uses = [A, HL]; let Defs = [A, FLAGS]; }
def CP_HLind : Inst8<"cp (hl)", 0xBE> { let Uses = [A, HL]; let Defs = [FLAGS]; }
}

// ALU A,n - the same operations against an immediate.
let mayLoad = 0, mayStore = 0, hasSideEffects = 0 in {
def ADD_A_n : Inst16<"add a,", 0xC6, Imm8Mode> { let Uses = [A]; let Defs = [A, FLAGS]; }
def ADC_A_n : Inst16<"adc a,", 0xCE, Imm8Mode> { let Uses = [A, FLAGS]; let Defs = [A, FLAGS]; }
def SUB_n : Inst16<"sub", 0xD6, Imm8Mode> { let Uses = [A]; let Defs = [A, FLAGS]; }
def SBC_A_n : Inst16<"sbc a,", 0xDE, Imm8Mode> { let Uses = [A, FLAGS]; let Defs = [A, FLAGS]; }
def AND_n : Inst16<"and", 0xE6, Imm8Mode> { let Uses = [A]; let Defs = [A, FLAGS]; }
def XOR_n : Inst16<"xor", 0xEE, Imm8Mode> { let Uses = [A]; let Defs = [A, FLAGS]; }
def OR_n : Inst16<"or", 0xF6, Imm8Mode> { let Uses = [A]; let Defs = [A, FLAGS]; }
def CP_n : Inst16<"cp", 0xFE, Imm8Mode> { let Uses = [A]; let Defs = [FLAGS]; }
}

// INC/DEC r - 8-bit INC/DEC set all flags except carry
let mayLoad = 0, mayStore = 0, hasSideEffects = 0 in {
def INC_r : INCDECr<"inc", 0b100>;
def DEC_r : INCDECr<"dec", 0b101>;
}

// The (HL) forms occupy the register field slot that names no register.
let mayLoad = true, mayStore = true, hasSideEffects = false in {
def INC_HLind : Inst8<"inc (hl)", 0x34> { let Uses = [HL]; let Defs = [FLAGS]; }
def DEC_HLind : Inst8<"dec (hl)", 0x35> { let Uses = [HL]; let Defs = [FLAGS]; }
}

let mayLoad = 0, mayStore = 0, hasSideEffects = 0 in {

//===----------------------------------------------------------------------===//
//  General-Purpose Arithmetic and CPU Control (common subset)
//===----------------------------------------------------------------------===//

def DAA : Inst8<"daa", 0x27> { let Uses = [A, FLAGS]; let Defs = [A, FLAGS]; }
def CPL : Inst8<"cpl", 0x2F> { let Uses = [A]; let Defs = [A, FLAGS]; }
def CCF : Inst8<"ccf", 0x3F> { let Uses = [FLAGS]; let Defs = [FLAGS]; }
def SCF : Inst8<"scf", 0x37> { let Defs = [FLAGS]; }
def NOP : Inst8<"nop", 0x00>;
}
def HALT : Inst8<"halt", 0x76>;
def DI : Inst8<"di", 0xF3>;
def EI : Inst8<"ei", 0xFB>;

//===----------------------------------------------------------------------===//
//  16-bit Arithmetic Group (common subset)
//===----------------------------------------------------------------------===//

// ADD HL,ss - Uses HL and the source register pair, defines HL (sets carry, H)
let mayLoad = 0, mayStore = 0, hasSideEffects = 0 in {
def ADD_HL_rr : ADDHLrr<"add"> { let Uses = [HL]; let Defs = [HL, FLAGS]; }
def ADD_HL_HL : Inst8<"add hl,hl", 0x29> { let Uses = [HL]; let Defs = [HL, FLAGS]; }
def ADD_HL_SP : Inst8<"add hl,sp", 0x39> { let Uses = [HL, SP]; let Defs = [HL, FLAGS]; }
}

// INC/DEC ss
let mayLoad = 0, mayStore = 0, hasSideEffects = 0 in {
def INC_rr : INCDECrr<"inc", 0b0011>;
def DEC_rr : INCDECrr<"dec", 0b1011>;
}

// SP is not a register operand in this backend, so it keeps its own pair.
def INC_SP : Inst8<"inc sp", 0x33> { let Uses = [SP]; let Defs = [SP]; }
def DEC_SP : Inst8<"dec sp", 0x3B> { let Uses = [SP]; let Defs = [SP]; }

//===----------------------------------------------------------------------===//
//  Rotate and Shift Group
//===----------------------------------------------------------------------===//

// Rotate and shift a register. The carry is both an input and an output for
// RL and RR, and an output only for the rest.
let mayLoad = 0, mayStore = 0, hasSideEffects = 0 in {
def RLC_r : CBRotate<"rlc", 0b000>;
def RRC_r : CBRotate<"rrc", 0b001>;
def RL_r : CBRotate<"rl", 0b010> { let Uses = [FLAGS]; let Defs = [FLAGS]; }
def RR_r : CBRotate<"rr", 0b011> { let Uses = [FLAGS]; let Defs = [FLAGS]; }
def SLA_r : CBRotate<"sla", 0b100>;
def SRA_r : CBRotate<"sra", 0b101>;
def SRL_r : CBRotate<"srl", 0b111>;
}

// Accumulator rotates — read and modify A
// RLCA/RRCA: don't read carry. RLA/RRA: read carry (rotate through carry).
let mayLoad = 0, mayStore = 0, hasSideEffects = 0 in {
def RLCA : Inst8<"rlca", 0x07> { let Uses = [A]; let Defs = [A, FLAGS]; }
def RRCA : Inst8<"rrca", 0x0F> { let Uses = [A]; let Defs = [A, FLAGS]; }
def RLA  : Inst8<"rla",  0x17> { let Uses = [A, FLAGS]; let Defs = [A, FLAGS]; }
def RRA  : Inst8<"rra",  0x1F> { let Uses = [A, FLAGS]; let Defs = [A, FLAGS]; }
}

// The same rotates and shifts against the byte HL points at. RL and RR read
// the carry; the rest only write it.
let mayLoad = true, mayStore = true, hasSideEffects = false in {
def RLC_HLind : InstCB<"rlc (hl)", 0x06> { let Uses = [HL]; let Defs = [FLAGS]; }
def RRC_HLind : InstCB<"rrc (hl)", 0x0E> { let Uses = [HL]; let Defs = [FLAGS]; }
def RL_HLind : InstCB<"rl (hl)", 0x16> { let Uses = [HL, FLAGS]; let Defs = [FLAGS]; }
def RR_HLind : InstCB<"rr (hl)", 0x1E> { let Uses = [HL, FLAGS]; let Defs = [FLAGS]; }
def SLA_HLind : InstCB<"sla (hl)", 0x26> { let Uses = [HL]; let Defs = [FLAGS]; }
def SRA_HLind : InstCB<"sra (hl)", 0x2E> { let Uses = [HL]; let Defs = [FLAGS]; }
def SRL_HLind : InstCB<"srl (hl)", 0x3E> { let Uses = [HL]; let Defs = [FLAGS]; }
}

//===----------------------------------------------------------------------===//
//  Bit Set, Reset, and Test Group (CB prefixed)
//===----------------------------------------------------------------------===//

// The bit number and the register are both operands.
let mayLoad = 0, mayStore = 0, hasSideEffects = 0 in {
def BIT_b_r : BITbr<"bit", 0b01> { let Defs = [FLAGS]; }
def RES_b_r : RESSETbr<"res", 0b10>;
def SET_b_r : RESSETbr<"set", 0b11>;
}

// BIT b,(HL) - Test a bit in memory
let mayLoad = true, mayStore = false, hasSideEffects = false in {
def BIT_0_HLind : InstCB<"bit 0,(hl)", 0x46>;
def BIT_1_HLind : InstCB<"bit 1,(hl)", 0x4E>;
def BIT_2_HLind : InstCB<"bit 2,(hl)", 0x56>;
def BIT_3_HLind : InstCB<"bit 3,(hl)", 0x5E>;
def BIT_4_HLind : InstCB<"bit 4,(hl)", 0x66>;
def BIT_5_HLind : InstCB<"bit 5,(hl)", 0x6E>;
def BIT_6_HLind : InstCB<"bit 6,(hl)", 0x76>;
def BIT_7_HLind : InstCB<"bit 7,(hl)", 0x7E>;
}

// RES b,(HL) - Reset a bit in memory
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

// SET b,(HL) - Set a bit in memory
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
