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

// LD r,r'
class LDrr<string mnemonic> : Z80Inst8<mnemonic # " $dst, $src"> {
  bits<3> dst;
  bits<3> src;
  let Inst{7-6} = 0b01;
  let Inst{5-3} = dst;
  let Inst{2-0} = src;
  let OutOperandList = (outs GR8:$dst);
  let InOperandList = (ins GR8:$src);
}

// LD r,n
class LDrn<string mnemonic> : Z80Inst16<mnemonic # " $dst, $imm"> {
  bits<3> dst;
  bits<8> imm;
  let Inst{7-6} = 0b00;
  let Inst{5-3} = dst;
  let Inst{2-0} = 0b110;
  let Inst{15-8} = imm;
  let OutOperandList = (outs GR8:$dst);
  let InOperandList = (ins imm8:$imm);
}

let mayLoad = 0, mayStore = 0, hasSideEffects = 0 in {
def LD_r_r : LDrr<"ld">;

def LD_r_n : LDrn<"ld"> {
  let isAsCheapAsAMove = 1;
  let isMoveImm = 1;
  let isReMaterializable = 1;
}
}

// LD r,(HL)
class LDrHLind<string mnemonic> : Z80Inst8<mnemonic # " $dst, (hl)"> {
  bits<3> dst;
  let Inst{7-6} = 0b01;
  let Inst{5-3} = dst;
  let Inst{2-0} = 0b110;
  let OutOperandList = (outs GR8:$dst);
  let InOperandList = (ins);
  let Uses = [HL];
}

let mayLoad = true, mayStore = false, hasSideEffects = false in
def LD_r_HLind : LDrHLind<"ld">;

// LD (HL),r
class LDHLindr<string mnemonic> : Z80Inst8<mnemonic # " (hl), $src"> {
  bits<3> src;
  let Inst{7-6} = 0b01;
  let Inst{5-3} = 0b110;
  let Inst{2-0} = src;
  let OutOperandList = (outs);
  let InOperandList = (ins GR8:$src);
  let Uses = [HL];
}

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

// LD rr,nn
class LDrrnn<string mnemonic> : Z80Inst24<mnemonic # " $dst, $imm"> {
  bits<2> dst;
  bits<16> imm;
  let Inst{7-6} = 0b00;
  let Inst{5-4} = dst;
  let Inst{3-0} = 0b0001;
  let Inst{23-8} = imm;
  let OutOperandList = (outs GR16:$dst);
  let InOperandList = (ins imm16:$imm);
}

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

// PUSH rr
class PUSHrr<Register rr>
    : Inst8<"push " # rr.AsmName, !or(0xC5, !shl(rr.HWEncoding{1-0}, 4))> {
  let Uses = [rr];
}

// POP rr
class POPrr<Register rr>
    : Inst8<"pop " # rr.AsmName, !or(0xC1, !shl(rr.HWEncoding{1-0}, 4))> {
  let Defs = [rr];
}

// PUSH/POP - stack memory access, no unmodeled side effects.
// Note: SP is not declared in Defs/Uses because LLVM's PEI handles
// SP adjustments separately. Peephole passes must check PUSH/POP
// opcodes explicitly when tracking SP.
let mayLoad = false, mayStore = true, hasSideEffects = false in {
def PUSH_BC : PUSHrr<BC>;
def PUSH_DE : PUSHrr<DE>;
def PUSH_HL : PUSHrr<HL>;
def PUSH_AF : PUSHrr<AF> { let Uses = [A, F]; }
}

let mayLoad = true, mayStore = false, hasSideEffects = false in {
def POP_BC : POPrr<BC>;
def POP_DE : POPrr<DE>;
def POP_HL : POPrr<HL>;
def POP_AF : POPrr<AF> { let Defs = [A, F]; }
}

//===----------------------------------------------------------------------===//
//  8-bit Arithmetic Group
//===----------------------------------------------------------------------===//

// ALU A,r for the eight accumulator operations
class ALUAr<string mnemonic, bits<3> op> : Z80Inst8<mnemonic # " $src"> {
  bits<3> src;
  let Inst{7-6} = 0b10;
  let Inst{5-3} = op;
  let Inst{2-0} = src;
  let OutOperandList = (outs);
  let InOperandList = (ins GR8:$src);
}

let mayLoad = 0, mayStore = 0, hasSideEffects = 0 in {
def ADD_A_r : ALUAr<"add a,", 0b000> { let Uses = [A]; let Defs = [A, F]; }
def ADC_A_r : ALUAr<"adc a,", 0b001> { let Uses = [A, F]; let Defs = [A, F]; }
def SUB_r   : ALUAr<"sub",    0b010> { let Uses = [A]; let Defs = [A, F]; }
def SBC_A_r : ALUAr<"sbc a,", 0b011> { let Uses = [A, F]; let Defs = [A, F]; }
def AND_r   : ALUAr<"and",    0b100> { let Uses = [A]; let Defs = [A, F]; }
def XOR_r   : ALUAr<"xor",    0b101> { let Uses = [A]; let Defs = [A, F]; }
def OR_r    : ALUAr<"or",     0b110> { let Uses = [A]; let Defs = [A, F]; }
def CP_r    : ALUAr<"cp",     0b111> { let Uses = [A]; let Defs = [F]; }
}

// ALU A,(HL) - the same operations against the byte HL points at.
let mayLoad = true, mayStore = false, hasSideEffects = false in {
def ADD_A_HLind : Inst8<"add a,(hl)", 0x86> { let Uses = [A, HL]; let Defs = [A, F]; }
def ADC_A_HLind : Inst8<"adc a,(hl)", 0x8E> { let Uses = [A, HL, F]; let Defs = [A, F]; }
def SUB_HLind : Inst8<"sub (hl)", 0x96> { let Uses = [A, HL]; let Defs = [A, F]; }
def SBC_A_HLind : Inst8<"sbc a,(hl)", 0x9E> { let Uses = [A, HL, F]; let Defs = [A, F]; }
def AND_HLind : Inst8<"and (hl)", 0xA6> { let Uses = [A, HL]; let Defs = [A, F]; }
def XOR_HLind : Inst8<"xor (hl)", 0xAE> { let Uses = [A, HL]; let Defs = [A, F]; }
def OR_HLind : Inst8<"or (hl)", 0xB6> { let Uses = [A, HL]; let Defs = [A, F]; }
def CP_HLind : Inst8<"cp (hl)", 0xBE> { let Uses = [A, HL]; let Defs = [F]; }
}

// ALU A,n - the same operations against an immediate.
let mayLoad = 0, mayStore = 0, hasSideEffects = 0 in {
def ADD_A_n : Inst16<"add a,", 0xC6, Imm8Mode> { let Uses = [A]; let Defs = [A, F]; }
def ADC_A_n : Inst16<"adc a,", 0xCE, Imm8Mode> { let Uses = [A, F]; let Defs = [A, F]; }
def SUB_n : Inst16<"sub", 0xD6, Imm8Mode> { let Uses = [A]; let Defs = [A, F]; }
def SBC_A_n : Inst16<"sbc a,", 0xDE, Imm8Mode> { let Uses = [A, F]; let Defs = [A, F]; }
def AND_n : Inst16<"and", 0xE6, Imm8Mode> { let Uses = [A]; let Defs = [A, F]; }
def XOR_n : Inst16<"xor", 0xEE, Imm8Mode> { let Uses = [A]; let Defs = [A, F]; }
def OR_n : Inst16<"or", 0xF6, Imm8Mode> { let Uses = [A]; let Defs = [A, F]; }
def CP_n : Inst16<"cp", 0xFE, Imm8Mode> { let Uses = [A]; let Defs = [F]; }
}

// INC r and DEC r. These set every flag except the carry.
class INCDECr<string mnemonic, bits<3> lo> : Z80Inst8<mnemonic # " $dst"> {
  bits<3> dst;
  let Inst{7-6} = 0b00;
  let Inst{5-3} = dst;
  let Inst{2-0} = lo;
  let OutOperandList = (outs GR8:$dst);
  let InOperandList = (ins GR8:$src);
  let Constraints = "$dst = $src";
  let Defs = [F];
}

let mayLoad = 0, mayStore = 0, hasSideEffects = 0 in {
def INC_r : INCDECr<"inc", 0b100>;
def DEC_r : INCDECr<"dec", 0b101>;
}

// The (HL) forms occupy the register field slot that names no register.
let mayLoad = true, mayStore = true, hasSideEffects = false in {
def INC_HLind : Inst8<"inc (hl)", 0x34> { let Uses = [HL]; let Defs = [F]; }
def DEC_HLind : Inst8<"dec (hl)", 0x35> { let Uses = [HL]; let Defs = [F]; }
}

let mayLoad = 0, mayStore = 0, hasSideEffects = 0 in {

//===----------------------------------------------------------------------===//
//  General-Purpose Arithmetic and CPU Control (common subset)
//===----------------------------------------------------------------------===//

def DAA : Inst8<"daa", 0x27> { let Uses = [A, F]; let Defs = [A, F]; }
def CPL : Inst8<"cpl", 0x2F> { let Uses = [A]; let Defs = [A, F]; }
def CCF : Inst8<"ccf", 0x3F> { let Uses = [F]; let Defs = [F]; }
def SCF : Inst8<"scf", 0x37> { let Defs = [F]; }
def NOP : Inst8<"nop", 0x00>;
}
def HALT : Inst8<"halt", 0x76>;
def DI : Inst8<"di", 0xF3>;
def EI : Inst8<"ei", 0xFB>;

//===----------------------------------------------------------------------===//
//  16-bit Arithmetic Group (common subset)
//===----------------------------------------------------------------------===//

// ADD HL,rr
class ADDHLrr<string mnemonic> : Z80Inst8<mnemonic # " hl, $src"> {
  bits<2> src;
  let Inst{7-6} = 0b00;
  let Inst{5-4} = src;
  let Inst{3-0} = 0b1001;
  let OutOperandList = (outs);
  let InOperandList = (ins GR16_BCDE:$src);
}

let mayLoad = 0, mayStore = 0, hasSideEffects = 0 in {
def ADD_HL_rr : ADDHLrr<"add"> { let Uses = [HL]; let Defs = [HL, F]; }
def ADD_HL_HL : Inst8<"add hl,hl", 0x29> { let Uses = [HL]; let Defs = [HL, F]; }
def ADD_HL_SP : Inst8<"add hl,sp", 0x39> { let Uses = [HL, SP]; let Defs = [HL, F]; }
}

// INC rr and DEC rr
class INCDECrr<string mnemonic, bits<4> lo> : Z80Inst8<mnemonic # " $dst"> {
  bits<2> dst;
  let Inst{7-6} = 0b00;
  let Inst{5-4} = dst;
  let Inst{3-0} = lo;
  let OutOperandList = (outs GR16:$dst);
  let InOperandList = (ins GR16:$src);
  let Constraints = "$dst = $src";
}

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

// RLC/RRC/RL/RR/SLA/SRA/SRL r
class CBRotate<string mnemonic, bits<3> op> : Z80Inst16<mnemonic # " $dst"> {
  bits<3> dst;
  let Inst{7-0} = 0xCB;
  let Inst{15-14} = 0b00;
  let Inst{13-11} = op;
  let Inst{10-8} = dst;
  let OutOperandList = (outs GR8:$dst);
  let InOperandList = (ins GR8:$src);
  let Constraints = "$dst = $src";
  let Defs = [F];
}

// Rotate and shift a register. The carry is both an input and an output for
// RL and RR, and an output only for the rest.
let mayLoad = 0, mayStore = 0, hasSideEffects = 0 in {
def RLC_r : CBRotate<"rlc", 0b000>;
def RRC_r : CBRotate<"rrc", 0b001>;
def RL_r : CBRotate<"rl", 0b010> { let Uses = [F]; let Defs = [F]; }
def RR_r : CBRotate<"rr", 0b011> { let Uses = [F]; let Defs = [F]; }
def SLA_r : CBRotate<"sla", 0b100>;
def SRA_r : CBRotate<"sra", 0b101>;
def SRL_r : CBRotate<"srl", 0b111>;
}

// Accumulator rotates — read and modify A
// RLCA/RRCA: don't read carry. RLA/RRA: read carry (rotate through carry).
let mayLoad = 0, mayStore = 0, hasSideEffects = 0 in {
def RLCA : Inst8<"rlca", 0x07> { let Uses = [A]; let Defs = [A, F]; }
def RRCA : Inst8<"rrca", 0x0F> { let Uses = [A]; let Defs = [A, F]; }
def RLA  : Inst8<"rla",  0x17> { let Uses = [A, F]; let Defs = [A, F]; }
def RRA  : Inst8<"rra",  0x1F> { let Uses = [A, F]; let Defs = [A, F]; }
}

// The same rotates and shifts against the byte HL points at. RL and RR read
// the carry; the rest only write it.
let mayLoad = true, mayStore = true, hasSideEffects = false in {
def RLC_HLind : InstCB<"rlc (hl)", 0x06> { let Uses = [HL]; let Defs = [F]; }
def RRC_HLind : InstCB<"rrc (hl)", 0x0E> { let Uses = [HL]; let Defs = [F]; }
def RL_HLind : InstCB<"rl (hl)", 0x16> { let Uses = [HL, F]; let Defs = [F]; }
def RR_HLind : InstCB<"rr (hl)", 0x1E> { let Uses = [HL, F]; let Defs = [F]; }
def SLA_HLind : InstCB<"sla (hl)", 0x26> { let Uses = [HL]; let Defs = [F]; }
def SRA_HLind : InstCB<"sra (hl)", 0x2E> { let Uses = [HL]; let Defs = [F]; }
def SRL_HLind : InstCB<"srl (hl)", 0x3E> { let Uses = [HL]; let Defs = [F]; }
}

//===----------------------------------------------------------------------===//
//  Bit Set, Reset, and Test Group (CB prefixed)
//===----------------------------------------------------------------------===//

// BIT b,r
class BITbr<string mnemonic, bits<2> op> : Z80Inst16<mnemonic # " $b, $src"> {
  bits<3> b;
  bits<3> src;
  let Inst{7-0} = 0xCB;
  let Inst{15-14} = op;
  let Inst{13-11} = b;
  let Inst{10-8} = src;
  let OutOperandList = (outs);
  let InOperandList = (ins imm3:$b, GR8:$src);
}

// RES b,r and SET b,r
class RESSETbr<string mnemonic, bits<2> op>
    : Z80Inst16<mnemonic # " $b, $dst"> {
  bits<3> b;
  bits<3> dst;
  let Inst{7-0} = 0xCB;
  let Inst{15-14} = op;
  let Inst{13-11} = b;
  let Inst{10-8} = dst;
  let OutOperandList = (outs GR8:$dst);
  let InOperandList = (ins imm3:$b, GR8:$src);
  let Constraints = "$dst = $src";
}

let mayLoad = 0, mayStore = 0, hasSideEffects = 0 in {
def BIT_b_r : BITbr<"bit", 0b01> { let Defs = [F]; }
def RES_b_r : RESSETbr<"res", 0b10>;
def SET_b_r : RESSETbr<"set", 0b11>;
}

// BIT/RES/SET b,(HL)
class BitOpHLind<string mnemonic, bits<2> op>
    : Z80Inst16<mnemonic # " $b, (hl)"> {
  bits<3> b;
  let Inst{7-0} = 0xCB;
  let Inst{15-14} = op;
  let Inst{13-11} = b;
  let Inst{10-8} = 0b110;
  let InOperandList = (ins imm3:$b);
}

let mayLoad = true, mayStore = false, hasSideEffects = false in
def BIT_b_HLind : BitOpHLind<"bit", 0b01> { let Uses = [HL]; let Defs = [F]; }

let mayLoad = true, mayStore = true, hasSideEffects = false in
def RES_b_HLind : BitOpHLind<"res", 0b10> { let Uses = [HL]; }

let mayLoad = true, mayStore = true, hasSideEffects = false in
def SET_b_HLind : BitOpHLind<"set", 0b11> { let Uses = [HL]; }

//===----------------------------------------------------------------------===//
//  Jump Group (common subset: NZ/Z/NC/C conditions only)
//===----------------------------------------------------------------------===//

// JP nn - Unconditional jump
let isBranch = true, isTerminator = true, isBarrier = true in
def JP_nn : Inst24<"jp", 0xC3, TargetMode>;

// JP cc,nn
class JPcond<Z80CondCode cc> : Inst24<"jp " # cc.Name # ",",
                                      !or(0xC2, !shl(cc.cc, 3)), TargetMode> {
  let isBranch = true;
  let isTerminator = true;
  let Uses = [F];
}

// NZ/Z/NC/C only, shared with SM83
def JP_NZ_nn : JPcond<CC_NZ>;
def JP_Z_nn  : JPcond<CC_Z>;
def JP_NC_nn : JPcond<CC_NC>;
def JP_C_nn  : JPcond<CC_C>;

// JR e - Relative jump
let isBranch = true, isTerminator = true, isBarrier = true in
def JR_e : Inst16<"jr", 0x18, RelativeMode>;

// JR cc,e
class JRcond<Z80CondCode cc> : Inst16<"jr " # cc.Name # ",",
                                      !or(0x20, !shl(cc.cc, 3)), RelativeMode> {
  let isBranch = true;
  let isTerminator = true;
  let Uses = [F];
}

def JR_NZ_e : JRcond<CC_NZ>;
def JR_Z_e  : JRcond<CC_Z>;
def JR_NC_e : JRcond<CC_NC>;
def JR_C_e  : JRcond<CC_C>;

// JP (HL) - Jump to address in HL
let isBranch = true, isTerminator = true, isBarrier = true in
def JP_HLind : Inst8<"jp (hl)", 0xE9> {
  let isIndirectBranch = true;
  let Uses = [HL];
}

//===----------------------------------------------------------------------===//
//  Call and Return Group (common subset: NZ/Z/NC/C conditions only)
//===----------------------------------------------------------------------===//

// CALL nn - Caller-saved registers are clobbered by calls.
// Argument registers are added dynamically as implicit uses in lowerCall.
// IY: caller-saved on Z80; harmless on SM83 where IY is reserved.
def CALL_nn : Inst24<"call", 0xCD, TargetMode> {
  let isCall = true;
  let Defs = [A, BC, DE, HL, IY, F];
}

// CALL cc,nn
class CALLcond<Z80CondCode cc> : Inst24<"call " # cc.Name # ",",
                                        !or(0xC4, !shl(cc.cc, 3)), TargetMode> {
  let isCall = true;
  let Uses = [F];
  let Defs = [A, BC, DE, HL, IY, F];
}

// NZ/Z/NC/C only, shared with SM83
def CALL_NZ_nn : CALLcond<CC_NZ>;
def CALL_Z_nn  : CALLcond<CC_Z>;
def CALL_NC_nn : CALLcond<CC_NC>;
def CALL_C_nn  : CALLcond<CC_C>;

// RET - Return from subroutine
// Note: Return value registers (A, HL, DE) are added as implicit uses
// dynamically in lowerReturn based on the actual return type.
let isReturn = true, isTerminator = true, isBarrier = true in
def RET : Inst8<"ret", 0xC9>;

// RET cc
class RETcond<Z80CondCode cc> : Inst8<"ret " # cc.Name,
                                      !or(0xC0, !shl(cc.cc, 3))> {
  let isReturn = true;
  let isTerminator = true;
  let Uses = [F];
}

// NZ/Z/NC/C only, shared with SM83
def RET_NZ : RETcond<CC_NZ>;
def RET_Z  : RETcond<CC_Z>;
def RET_NC : RETcond<CC_NC>;
def RET_C  : RETcond<CC_C>;

// RST p
class RSTvec<bits<3> t> : Inst8<"rst " # !mul(t, 8),
                                !or(0xC7, !shl(t, 3))> {
  let isCall = true;
}

def RST_00 : RSTvec<0>;
def RST_08 : RSTvec<1>;
def RST_10 : RSTvec<2>;
def RST_18 : RSTvec<3>;
def RST_20 : RSTvec<4>;
def RST_28 : RSTvec<5>;
def RST_30 : RSTvec<6>;
def RST_38 : RSTvec<7>;
