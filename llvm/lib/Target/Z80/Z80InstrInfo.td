//===-- Z80InstrInfo.td - Z80 Instruction defs --------------*- tablegen -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes the Z80 instructions in TableGen format.
//
// Z80 instruction encoding:
//   - Unprefixed: 1-3 bytes
//   - CB prefix: Bit manipulation (2 bytes)
//   - DD prefix: IX operations (2-4 bytes)
//   - FD prefix: IY operations (2-4 bytes)
//   - ED prefix: Extended operations (2-4 bytes)
//   - DD CB / FD CB: Indexed bit operations (4 bytes)
//
//===----------------------------------------------------------------------===//

include "Z80InstrFormats.td"
include "Z80InstrCommon.td"

//===----------------------------------------------------------------------===//
// Z80-only instructions
// These use ED/DD/FD prefixes, exchange operations, block transfer/search,
// I/O operations, sign/parity condition codes, or opcodes reused by SM83.
//===----------------------------------------------------------------------===//

let Predicates = [HasZ80] in {

//===----------------------------------------------------------------------===//
//  8-bit Load Group (Z80-only)
//===----------------------------------------------------------------------===//

// LD A,(nn) / LD (nn),A - Direct addressing
// (SM83 reuses opcodes 0x32/0x3A for LD (HL-),A / LD A,(HL-))
let mayLoad = true, mayStore = false, hasSideEffects = false in
def LD_A_nnind : Inst24<"ld a,", 0x3A, DirectMode> { let Defs = [A]; }
let mayLoad = false, mayStore = true, hasSideEffects = false in
def LD_nnind_A : Inst24<"ld", 0x32, DirectMode> {
  let AsmString = "ld ( $imm ),a";
  let Uses = [A];
}

// LD A,I / LD A,R / LD I,A / LD R,A - Special registers (ED prefix)
def LD_A_I : InstED8<"ld a,i", 0x57>;
def LD_A_R : InstED8<"ld a,r", 0x5F>;
def LD_I_A : InstED8<"ld i,a", 0x47>;
def LD_R_A : InstED8<"ld r,a", 0x4F>;

//===----------------------------------------------------------------------===//
//  16-bit Load Group (Z80-only)
//===----------------------------------------------------------------------===//

// LD HL,(nn) - Load HL from direct address
// (SM83 reuses opcode 0x2A for LD A,(HL+))
let mayLoad = true, mayStore = false, hasSideEffects = false in
def LD_HL_nnind : Inst24<"ld hl,", 0x2A, DirectMode> { let Defs = [HL]; }

// LD (nn),HL - Store HL to direct address
// (SM83 reuses opcode 0x22 for LD (HL+),A)
let mayLoad = false, mayStore = true, hasSideEffects = false in
def LD_nnind_HL : Inst24<"ld", 0x22, DirectMode> {
  let AsmString = "ld ( $imm ),hl";
  let Uses = [HL];
}

// LD dd,(nn) / LD (nn),dd - ED prefixed
let mayLoad = true, mayStore = false, hasSideEffects = false in {
def LD_BC_nnind : InstED24<"ld bc,", 0x4B, DirectMode> { let Defs = [BC]; }
def LD_DE_nnind : InstED24<"ld de,", 0x5B, DirectMode> { let Defs = [DE]; }
def LD_SP_nnind : InstED24<"ld sp,", 0x7B, DirectMode> { let Defs = [SP]; }
}

let mayLoad = false, mayStore = true, hasSideEffects = false in {
def LD_nnind_BC : InstED24<"ld", 0x43, DirectMode> { let AsmString = "ld ( $imm ),bc"; let Uses = [BC]; }
def LD_nnind_DE : InstED24<"ld", 0x53, DirectMode> { let AsmString = "ld ( $imm ),de"; let Uses = [DE]; }
def LD_nnind_SP : InstED24<"ld", 0x73, DirectMode> { let AsmString = "ld ( $imm ),sp"; let Uses = [SP]; }
}

//===----------------------------------------------------------------------===//
//  Exchange Group
//===----------------------------------------------------------------------===//

def EX_DE_HL : Inst8<"ex de,hl", 0xEB> { let Uses = [DE, HL]; let Defs = [DE, HL]; }
def EX_AF_AF : Inst8<"ex af,af'", 0x08>;
def EXX : Inst8<"exx", 0xD9>;
let mayLoad = true, mayStore = true, hasSideEffects = false in
def EX_SPind_HL : Inst8<"ex (sp),hl", 0xE3> { let Uses = [SP, HL]; let Defs = [HL]; }

//===----------------------------------------------------------------------===//
//  Block Transfer and Search Group (ED prefixed)
//===----------------------------------------------------------------------===//

// Block transfer: read (HL), write (DE), decrement BC, inc/dec HL and DE
let mayLoad = true, mayStore = true in {
def LDI  : InstED8<"ldi",  0xA0> { let Uses = [HL, DE, BC]; let Defs = [HL, DE, BC, FLAGS]; }
def LDIR : InstED8<"ldir", 0xB0> { let Uses = [HL, DE, BC]; let Defs = [HL, DE, BC, FLAGS]; }
def LDD  : InstED8<"ldd",  0xA8> { let Uses = [HL, DE, BC]; let Defs = [HL, DE, BC, FLAGS]; }
def LDDR : InstED8<"lddr", 0xB8> { let Uses = [HL, DE, BC]; let Defs = [HL, DE, BC, FLAGS]; }
}

// Block compare: read (HL), compare with A, decrement BC, inc/dec HL
let mayLoad = true, mayStore = false in {
def CPI  : InstED8<"cpi",  0xA1> { let Uses = [HL, A, BC]; let Defs = [HL, BC, FLAGS]; }
def CPIR : InstED8<"cpir", 0xB1> { let Uses = [HL, A, BC]; let Defs = [HL, BC, FLAGS]; }
def CPD  : InstED8<"cpd",  0xA9> { let Uses = [HL, A, BC]; let Defs = [HL, BC, FLAGS]; }
def CPDR : InstED8<"cpdr", 0xB9> { let Uses = [HL, A, BC]; let Defs = [HL, BC, FLAGS]; }
}

//===----------------------------------------------------------------------===//
//  General-Purpose Arithmetic and CPU Control (Z80-only)
//===----------------------------------------------------------------------===//

def NEG : InstED8<"neg", 0x44> { let Uses = [A]; let Defs = [A, FLAGS]; }

// IM instructions
def IM_0 : InstED8<"im 0", 0x46>;
def IM_1 : InstED8<"im 1", 0x56>;
def IM_2 : InstED8<"im 2", 0x5E>;

//===----------------------------------------------------------------------===//
//  16-bit Arithmetic Group (Z80-only: ED prefixed)
//===----------------------------------------------------------------------===//

// ADC HL,ss (ED prefixed) - Add with carry (reads carry flag)
def ADC_HL_BC : InstED8<"adc hl,bc", 0x4A> { let Uses = [HL, BC, FLAGS]; let Defs = [HL, FLAGS]; }
def ADC_HL_DE : InstED8<"adc hl,de", 0x5A> { let Uses = [HL, DE, FLAGS]; let Defs = [HL, FLAGS]; }
def ADC_HL_HL : InstED8<"adc hl,hl", 0x6A> { let Uses = [HL, FLAGS]; let Defs = [HL, FLAGS]; }
def ADC_HL_SP : InstED8<"adc hl,sp", 0x7A> { let Uses = [HL, SP, FLAGS]; let Defs = [HL, FLAGS]; }

// SBC HL,ss (ED prefixed) - Subtract with carry (reads carry flag)
def SBC_HL_BC : InstED8<"sbc hl,bc", 0x42> { let Uses = [HL, BC, FLAGS]; let Defs = [HL, FLAGS]; }
def SBC_HL_DE : InstED8<"sbc hl,de", 0x52> { let Uses = [HL, DE, FLAGS]; let Defs = [HL, FLAGS]; }
def SBC_HL_HL : InstED8<"sbc hl,hl", 0x62> { let Uses = [HL, FLAGS]; let Defs = [HL, FLAGS]; }
def SBC_HL_SP : InstED8<"sbc hl,sp", 0x72> { let Uses = [HL, SP, FLAGS]; let Defs = [HL, FLAGS]; }

//===----------------------------------------------------------------------===//
//  Rotate and Shift Group (Z80-only: ED prefixed)
//===----------------------------------------------------------------------===//

// RLD / RRD (ED prefixed) - set flags
let mayLoad = true, mayStore = true, hasSideEffects = false in {
def RLD : InstED8<"rld", 0x6F> { let Uses = [HL, A]; let Defs = [A, FLAGS]; }
def RRD : InstED8<"rrd", 0x67> { let Uses = [HL, A]; let Defs = [A, FLAGS]; }
}

//===----------------------------------------------------------------------===//
//  Jump Group (Z80-only: sign/parity conditions + DJNZ)
//===----------------------------------------------------------------------===//

// JP cc,nn - Sign/parity conditions (Z80-only)
def JP_PO_nn : JPcond<CC_PO>;
def JP_PE_nn : JPcond<CC_PE>;
def JP_P_nn  : JPcond<CC_P>;
def JP_M_nn  : JPcond<CC_M>;

// DJNZ e - Decrement B and jump if not zero
def DJNZ_e : Inst16<"djnz", 0x10, RelativeMode>, InstConditionalBranch;

//===----------------------------------------------------------------------===//
//  Call and Return Group (Z80-only)
//===----------------------------------------------------------------------===//

// CALL_IY - Indirect call through IY register.
// Pseudo-instruction that expands to "call __call_iy" in expandPostRAPseudo.
// The runtime __call_iy trampoline does "jp (iy)".
// IY holds the function pointer; HL/DE are free for arguments.
let isPseudo = true in
def CALL_IY : Z80Inst<"call\t__call_iy">, InstCall {
  let InOperandList = (ins);
  let Uses = [IY];
  let Defs = [A, BC, DE, HL, IY, FLAGS];
}

// CALL cc,nn - Sign/parity conditions (Z80-only)
def CALL_PO_nn : CALLcond<CC_PO>;
def CALL_PE_nn : CALLcond<CC_PE>;
def CALL_P_nn  : CALLcond<CC_P>;
def CALL_M_nn  : CALLcond<CC_M>;

// RET cc - Sign/parity conditions (Z80-only)
def RET_PO : RETcond<CC_PO>;
def RET_PE : RETcond<CC_PE>;
def RET_P  : RETcond<CC_P>;
def RET_M  : RETcond<CC_M>;

// RETI / RETN (ED prefix)
def RETI : InstED8<"reti", 0x4D>, InstReturn;
def RETN : InstED8<"retn", 0x45>, InstReturn;

//===----------------------------------------------------------------------===//
//  Input and Output Group
//===----------------------------------------------------------------------===//

// IN A,(n)
def IN_A_n : Inst16<"in a,", 0xDB, PortMode> {
  let AsmString = "in a, ( $imm )";
}

// OUT (n),A
def OUT_n_A : Inst16<"out", 0xD3, PortMode> {
  let AsmString = "out ( $imm ), a";
}

// IN r,(C) - ED prefixed (sets flags)
// These instructions read from port specified by C and store in the register
let Uses = [C] in {
  def IN_B_C : InstED8<"in b,(c)", 0x40> { let Defs = [B, FLAGS]; }
  def IN_C_C : InstED8<"in c,(c)", 0x48> { let Defs = [C, FLAGS]; }
  def IN_D_C : InstED8<"in d,(c)", 0x50> { let Defs = [D, FLAGS]; }
  def IN_E_C : InstED8<"in e,(c)", 0x58> { let Defs = [E, FLAGS]; }
  def IN_H_C : InstED8<"in h,(c)", 0x60> { let Defs = [H, FLAGS]; }
  def IN_L_C : InstED8<"in l,(c)", 0x68> { let Defs = [L, FLAGS]; }
  def IN_A_C : InstED8<"in a,(c)", 0x78> { let Defs = [A, FLAGS]; }
}

// OUT (C),r - ED prefixed
// These instructions write the register value to port specified by C
let Uses = [C] in {
  def OUT_C_B : InstED8<"out (c),b", 0x41> { let Uses = [C, B]; }
  def OUT_C_C : InstED8<"out (c),c", 0x49> { let Uses = [C]; }
  def OUT_C_D : InstED8<"out (c),d", 0x51> { let Uses = [C, D]; }
  def OUT_C_E : InstED8<"out (c),e", 0x59> { let Uses = [C, E]; }
  def OUT_C_H : InstED8<"out (c),h", 0x61> { let Uses = [C, H]; }
  def OUT_C_L : InstED8<"out (c),l", 0x69> { let Uses = [C, L]; }
  def OUT_C_A : InstED8<"out (c),a", 0x79> { let Uses = [C, A]; }
}

// Block I/O (set flags)
def INI  : InstED8<"ini",  0xA2> { let Defs = [FLAGS]; }
def INIR : InstED8<"inir", 0xB2> { let Defs = [FLAGS]; }
def IND  : InstED8<"ind",  0xAA> { let Defs = [FLAGS]; }
def INDR : InstED8<"indr", 0xBA> { let Defs = [FLAGS]; }

def OUTI : InstED8<"outi", 0xA3> { let Defs = [FLAGS]; }
def OTIR : InstED8<"otir", 0xB3> { let Defs = [FLAGS]; }
def OUTD : InstED8<"outd", 0xAB> { let Defs = [FLAGS]; }
def OTDR : InstED8<"otdr", 0xBB> { let Defs = [FLAGS]; }

} // Predicates = [HasZ80]

//===----------------------------------------------------------------------===//
//  IX/IY Instructions (DD/FD prefixed) - Selected examples
//===----------------------------------------------------------------------===//

let Predicates = [HasZ80] in {

// LD IX,nn / LD IY,nn
def LD_IX_nn : InstDD24<"ld ix,", 0x21, Imm16Mode> { let Defs = [IX]; }
def LD_IY_nn : InstFD24<"ld iy,", 0x21, Imm16Mode> { let Defs = [IY]; }

// PUSH IX / PUSH IY
def PUSH_IX : InstDD8<"push ix", 0xE5> { let Uses = [IX]; }
def PUSH_IY : InstFD8<"push iy", 0xE5> { let Uses = [IY]; }

// POP IX / POP IY
def POP_IX : InstDD8<"pop ix", 0xE1> { let Defs = [IX]; }
def POP_IY : InstFD8<"pop iy", 0xE1> { let Defs = [IY]; }

// JP (IX) / JP (IY)
def JP_IXind : InstDD8<"jp (ix)", 0xE9>, InstUnconditionalBranch {
  let isIndirectBranch = true;
  let Uses = [IX];
}
def JP_IYind : InstFD8<"jp (iy)", 0xE9>, InstUnconditionalBranch {
  let isIndirectBranch = true;
  let Uses = [IY];
}

// LD SP,IX / LD SP,IY
def LD_SP_IX : InstDD8<"ld sp,ix", 0xF9> { let Uses = [IX]; let Defs = [SP]; }
def LD_SP_IY : InstFD8<"ld sp,iy", 0xF9> { let Uses = [IY]; let Defs = [SP]; }

// INC IX / INC IY / DEC IX / DEC IY
def INC_IX : InstDD8<"inc ix", 0x23> { let Uses = [IX]; let Defs = [IX]; }
def INC_IY : InstFD8<"inc iy", 0x23> { let Uses = [IY]; let Defs = [IY]; }
def DEC_IX : InstDD8<"dec ix", 0x2B> { let Uses = [IX]; let Defs = [IX]; }
def DEC_IY : InstFD8<"dec iy", 0x2B> { let Uses = [IY]; let Defs = [IY]; }

// ADD IX,pp / ADD IY,rr (sets carry, H)
def ADD_IX_BC : InstDD8<"add ix,bc", 0x09> { let Uses = [IX, BC]; let Defs = [IX, FLAGS]; }
def ADD_IX_DE : InstDD8<"add ix,de", 0x19> { let Uses = [IX, DE]; let Defs = [IX, FLAGS]; }
def ADD_IX_IX : InstDD8<"add ix,ix", 0x29> { let Uses = [IX];     let Defs = [IX, FLAGS]; }
def ADD_IX_SP : InstDD8<"add ix,sp", 0x39> { let Uses = [IX, SP]; let Defs = [IX, FLAGS]; }

def ADD_IY_BC : InstFD8<"add iy,bc", 0x09> { let Uses = [IY, BC]; let Defs = [IY, FLAGS]; }
def ADD_IY_DE : InstFD8<"add iy,de", 0x19> { let Uses = [IY, DE]; let Defs = [IY, FLAGS]; }
def ADD_IY_IY : InstFD8<"add iy,iy", 0x29> { let Uses = [IY];     let Defs = [IY, FLAGS]; }
def ADD_IY_SP : InstFD8<"add iy,sp", 0x39> { let Uses = [IY, SP]; let Defs = [IY, FLAGS]; }

// EX (SP),IX / EX (SP),IY
let mayLoad = true, mayStore = true, hasSideEffects = false in {
def EX_SPind_IX : InstDD8<"ex (sp),ix", 0xE3> { let Uses = [SP, IX]; let Defs = [IX]; }
def EX_SPind_IY : InstFD8<"ex (sp),iy", 0xE3> { let Uses = [SP, IY]; let Defs = [IY]; }
}

//===----------------------------------------------------------------------===//
// Indexed addressing: LD r,(IX+d) / LD r,(IY+d)
// Opcode: DD [46|4E|56|5E|66|6E|7E] d for IX
// Opcode: FD [46|4E|56|5E|66|6E|7E] d for IY
//===----------------------------------------------------------------------===//

// LD r,(IX+d) — loads define the destination register
def LD_B_IXd : InstDD16<"ld b,", 0x46, IndexedIXMode> { let Defs = [B]; let mayLoad = 1; let mayStore = 0; let hasSideEffects = 0; }
def LD_C_IXd : InstDD16<"ld c,", 0x4E, IndexedIXMode> { let Defs = [C]; let mayLoad = 1; let mayStore = 0; let hasSideEffects = 0; }
def LD_D_IXd : InstDD16<"ld d,", 0x56, IndexedIXMode> { let Defs = [D]; let mayLoad = 1; let mayStore = 0; let hasSideEffects = 0; }
def LD_E_IXd : InstDD16<"ld e,", 0x5E, IndexedIXMode> { let Defs = [E]; let mayLoad = 1; let mayStore = 0; let hasSideEffects = 0; }
def LD_H_IXd : InstDD16<"ld h,", 0x66, IndexedIXMode> { let Defs = [H]; let mayLoad = 1; let mayStore = 0; let hasSideEffects = 0; }
def LD_L_IXd : InstDD16<"ld l,", 0x6E, IndexedIXMode> { let Defs = [L]; let mayLoad = 1; let mayStore = 0; let hasSideEffects = 0; }
def LD_A_IXd : InstDD16<"ld a,", 0x7E, IndexedIXMode> { let Defs = [A]; let mayLoad = 1; let mayStore = 0; let hasSideEffects = 0; }

// LD r,(IY+d) — loads define the destination register
def LD_B_IYd : InstFD16<"ld b,", 0x46, IndexedIYMode> { let Defs = [B]; let mayLoad = 1; let mayStore = 0; let hasSideEffects = 0; }
def LD_C_IYd : InstFD16<"ld c,", 0x4E, IndexedIYMode> { let Defs = [C]; let mayLoad = 1; let mayStore = 0; let hasSideEffects = 0; }
def LD_D_IYd : InstFD16<"ld d,", 0x56, IndexedIYMode> { let Defs = [D]; let mayLoad = 1; let mayStore = 0; let hasSideEffects = 0; }
def LD_E_IYd : InstFD16<"ld e,", 0x5E, IndexedIYMode> { let Defs = [E]; let mayLoad = 1; let mayStore = 0; let hasSideEffects = 0; }
def LD_H_IYd : InstFD16<"ld h,", 0x66, IndexedIYMode> { let Defs = [H]; let mayLoad = 1; let mayStore = 0; let hasSideEffects = 0; }
def LD_L_IYd : InstFD16<"ld l,", 0x6E, IndexedIYMode> { let Defs = [L]; let mayLoad = 1; let mayStore = 0; let hasSideEffects = 0; }
def LD_A_IYd : InstFD16<"ld a,", 0x7E, IndexedIYMode> { let Defs = [A]; let mayLoad = 1; let mayStore = 0; let hasSideEffects = 0; }

//===----------------------------------------------------------------------===//
// Indexed addressing: LD (IX+d),r / LD (IY+d),r
// Opcode: DD [70|71|72|73|74|75|77] d for IX
// Opcode: FD [70|71|72|73|74|75|77] d for IY
//===----------------------------------------------------------------------===//

// LD (IX+d),r — stores use the source register
def LD_IXd_B : InstDD16<"ld", 0x70, IndexedIXMode> { let Uses = [B]; let AsmString = "ld ( ix + $imm ), b"; let mayLoad = 0; let mayStore = 1; let hasSideEffects = 0; }
def LD_IXd_C : InstDD16<"ld", 0x71, IndexedIXMode> { let Uses = [C]; let AsmString = "ld ( ix + $imm ), c"; let mayLoad = 0; let mayStore = 1; let hasSideEffects = 0; }
def LD_IXd_D : InstDD16<"ld", 0x72, IndexedIXMode> { let Uses = [D]; let AsmString = "ld ( ix + $imm ), d"; let mayLoad = 0; let mayStore = 1; let hasSideEffects = 0; }
def LD_IXd_E : InstDD16<"ld", 0x73, IndexedIXMode> { let Uses = [E]; let AsmString = "ld ( ix + $imm ), e"; let mayLoad = 0; let mayStore = 1; let hasSideEffects = 0; }
def LD_IXd_H : InstDD16<"ld", 0x74, IndexedIXMode> { let Uses = [H]; let AsmString = "ld ( ix + $imm ), h"; let mayLoad = 0; let mayStore = 1; let hasSideEffects = 0; }
def LD_IXd_L : InstDD16<"ld", 0x75, IndexedIXMode> { let Uses = [L]; let AsmString = "ld ( ix + $imm ), l"; let mayLoad = 0; let mayStore = 1; let hasSideEffects = 0; }
def LD_IXd_A : InstDD16<"ld", 0x77, IndexedIXMode> { let Uses = [A]; let AsmString = "ld ( ix + $imm ), a"; let mayLoad = 0; let mayStore = 1; let hasSideEffects = 0; }

// LD (IY+d),r — stores use the source register
def LD_IYd_B : InstFD16<"ld", 0x70, IndexedIYMode> { let Uses = [B]; let AsmString = "ld ( iy + $imm ), b"; let mayLoad = 0; let mayStore = 1; let hasSideEffects = 0; }
def LD_IYd_C : InstFD16<"ld", 0x71, IndexedIYMode> { let Uses = [C]; let AsmString = "ld ( iy + $imm ), c"; let mayLoad = 0; let mayStore = 1; let hasSideEffects = 0; }
def LD_IYd_D : InstFD16<"ld", 0x72, IndexedIYMode> { let Uses = [D]; let AsmString = "ld ( iy + $imm ), d"; let mayLoad = 0; let mayStore = 1; let hasSideEffects = 0; }
def LD_IYd_E : InstFD16<"ld", 0x73, IndexedIYMode> { let Uses = [E]; let AsmString = "ld ( iy + $imm ), e"; let mayLoad = 0; let mayStore = 1; let hasSideEffects = 0; }
def LD_IYd_H : InstFD16<"ld", 0x74, IndexedIYMode> { let Uses = [H]; let AsmString = "ld ( iy + $imm ), h"; let mayLoad = 0; let mayStore = 1; let hasSideEffects = 0; }
def LD_IYd_L : InstFD16<"ld", 0x75, IndexedIYMode> { let Uses = [L]; let AsmString = "ld ( iy + $imm ), l"; let mayLoad = 0; let mayStore = 1; let hasSideEffects = 0; }
def LD_IYd_A : InstFD16<"ld", 0x77, IndexedIYMode> { let Uses = [A]; let AsmString = "ld ( iy + $imm ), a"; let mayLoad = 0; let mayStore = 1; let hasSideEffects = 0; }

//===----------------------------------------------------------------------===//
// Indexed addressing: LD (IX+d),n / LD (IY+d),n - Store immediate to indexed
// Opcode: DD 36 d n for IX
// Opcode: FD 36 d n for IY
//===----------------------------------------------------------------------===//

def LD_IXd_n : InstDD16Imm<"ld", 0x36> { let AsmString = "ld ( ix + $disp ), $val"; let mayLoad = 0; let mayStore = 1; let hasSideEffects = 0; }
def LD_IYd_n : InstFD16Imm<"ld", 0x36> { let AsmString = "ld ( iy + $disp ), $val"; let mayLoad = 0; let mayStore = 1; let hasSideEffects = 0; }

//===----------------------------------------------------------------------===//
//  IX-indexed ALU instructions (DD prefix)
//===----------------------------------------------------------------------===//

let mayLoad = 1, mayStore = 0, hasSideEffects = 0 in {
  def ADD_A_IXd : InstDD16<"add a,", 0x86, IndexedIXMode> {
    let Uses = [A, IX]; let Defs = [A, FLAGS];
  }
  def ADC_A_IXd : InstDD16<"adc a,", 0x8E, IndexedIXMode> {
    let Uses = [A, IX, FLAGS]; let Defs = [A, FLAGS];
  }
  def SUB_IXd : InstDD16<"sub", 0x96, IndexedIXMode> {
    let AsmString = "sub ( ix + $imm )";
    let Uses = [A, IX]; let Defs = [A, FLAGS];
  }
  def SBC_A_IXd : InstDD16<"sbc a,", 0x9E, IndexedIXMode> {
    let Uses = [A, IX, FLAGS]; let Defs = [A, FLAGS];
  }
  def AND_IXd : InstDD16<"and", 0xA6, IndexedIXMode> {
    let AsmString = "and ( ix + $imm )";
    let Uses = [A, IX]; let Defs = [A, FLAGS];
  }
  def XOR_IXd : InstDD16<"xor", 0xAE, IndexedIXMode> {
    let AsmString = "xor ( ix + $imm )";
    let Uses = [A, IX]; let Defs = [A, FLAGS];
  }
  def OR_IXd : InstDD16<"or", 0xB6, IndexedIXMode> {
    let AsmString = "or ( ix + $imm )";
    let Uses = [A, IX]; let Defs = [A, FLAGS];
  }
  def CP_IXd : InstDD16<"cp", 0xBE, IndexedIXMode> {
    let AsmString = "cp ( ix + $imm )";
    let Uses = [A, IX]; let Defs = [FLAGS];
  }
}

//===----------------------------------------------------------------------===//
//  IY-indexed ALU instructions (FD prefix)
//===----------------------------------------------------------------------===//

let mayLoad = 1, mayStore = 0, hasSideEffects = 0 in {
  def ADD_A_IYd : InstFD16<"add a,", 0x86, IndexedIYMode> {
    let Uses = [A, IY]; let Defs = [A, FLAGS];
  }
  def ADC_A_IYd : InstFD16<"adc a,", 0x8E, IndexedIYMode> {
    let Uses = [A, IY, FLAGS]; let Defs = [A, FLAGS];
  }
  def SUB_IYd : InstFD16<"sub", 0x96, IndexedIYMode> {
    let AsmString = "sub ( iy + $imm )";
    let Uses = [A, IY]; let Defs = [A, FLAGS];
  }
  def SBC_A_IYd : InstFD16<"sbc a,", 0x9E, IndexedIYMode> {
    let Uses = [A, IY, FLAGS]; let Defs = [A, FLAGS];
  }
  def AND_IYd : InstFD16<"and", 0xA6, IndexedIYMode> {
    let AsmString = "and ( iy + $imm )";
    let Uses = [A, IY]; let Defs = [A, FLAGS];
  }
  def XOR_IYd : InstFD16<"xor", 0xAE, IndexedIYMode> {
    let AsmString = "xor ( iy + $imm )";
    let Uses = [A, IY]; let Defs = [A, FLAGS];
  }
  def OR_IYd : InstFD16<"or", 0xB6, IndexedIYMode> {
    let AsmString = "or ( iy + $imm )";
    let Uses = [A, IY]; let Defs = [A, FLAGS];
  }
  def CP_IYd : InstFD16<"cp", 0xBE, IndexedIYMode> {
    let AsmString = "cp ( iy + $imm )";
    let Uses = [A, IY]; let Defs = [FLAGS];
  }
}

//===----------------------------------------------------------------------===//
//  IX/IY-indexed INC/DEC (DD/FD prefix)
//===----------------------------------------------------------------------===//

let mayLoad = 1, mayStore = 1, hasSideEffects = 0 in {
  def INC_IXd : InstDD16<"inc", 0x34, IndexedIXMode> {
    let AsmString = "inc ( ix + $imm )";
    let Uses = [IX]; let Defs = [FLAGS];
  }
  def DEC_IXd : InstDD16<"dec", 0x35, IndexedIXMode> {
    let AsmString = "dec ( ix + $imm )";
    let Uses = [IX]; let Defs = [FLAGS];
  }
  def INC_IYd : InstFD16<"inc", 0x34, IndexedIYMode> {
    let AsmString = "inc ( iy + $imm )";
    let Uses = [IY]; let Defs = [FLAGS];
  }
  def DEC_IYd : InstFD16<"dec", 0x35, IndexedIYMode> {
    let AsmString = "dec ( iy + $imm )";
    let Uses = [IY]; let Defs = [FLAGS];
  }
}

//===----------------------------------------------------------------------===//
//  DDCB/FDCB indexed rotate/shift instructions (4-byte)
//===----------------------------------------------------------------------===//

// Rotate/shift (IX+d) — read and write memory
let mayLoad = 1, mayStore = 1, hasSideEffects = 0 in {
  def RLC_IXd : InstDDCB<"rlc", 0x06> { let Uses = [IX]; let Defs = [FLAGS]; }
  def RRC_IXd : InstDDCB<"rrc", 0x0E> { let Uses = [IX]; let Defs = [FLAGS]; }
  def RL_IXd  : InstDDCB<"rl",  0x16> { let Uses = [IX, FLAGS]; let Defs = [FLAGS]; }
  def RR_IXd  : InstDDCB<"rr",  0x1E> { let Uses = [IX, FLAGS]; let Defs = [FLAGS]; }
  def SLA_IXd : InstDDCB<"sla", 0x26> { let Uses = [IX]; let Defs = [FLAGS]; }
  def SRA_IXd : InstDDCB<"sra", 0x2E> { let Uses = [IX]; let Defs = [FLAGS]; }
  def SRL_IXd : InstDDCB<"srl", 0x3E> { let Uses = [IX]; let Defs = [FLAGS]; }
}

// Rotate/shift (IY+d) — read and write memory
let mayLoad = 1, mayStore = 1, hasSideEffects = 0 in {
  def RLC_IYd : InstFDCB<"rlc", 0x06> { let Uses = [IY]; let Defs = [FLAGS]; }
  def RRC_IYd : InstFDCB<"rrc", 0x0E> { let Uses = [IY]; let Defs = [FLAGS]; }
  def RL_IYd  : InstFDCB<"rl",  0x16> { let Uses = [IY, FLAGS]; let Defs = [FLAGS]; }
  def RR_IYd  : InstFDCB<"rr",  0x1E> { let Uses = [IY, FLAGS]; let Defs = [FLAGS]; }
  def SLA_IYd : InstFDCB<"sla", 0x26> { let Uses = [IY]; let Defs = [FLAGS]; }
  def SRA_IYd : InstFDCB<"sra", 0x2E> { let Uses = [IY]; let Defs = [FLAGS]; }
  def SRL_IYd : InstFDCB<"srl", 0x3E> { let Uses = [IY]; let Defs = [FLAGS]; }
}

//===----------------------------------------------------------------------===//
//  DDCB/FDCB indexed BIT/SET/RES instructions (4-byte)
//===----------------------------------------------------------------------===//

// BIT b,(IX+d) — read memory only
let mayLoad = 1, mayStore = 0, hasSideEffects = 0 in {
  def BIT_0_IXd : InstDDCB<"bit 0,", 0x46> { let Uses = [IX]; let Defs = [FLAGS]; }
  def BIT_1_IXd : InstDDCB<"bit 1,", 0x4E> { let Uses = [IX]; let Defs = [FLAGS]; }
  def BIT_2_IXd : InstDDCB<"bit 2,", 0x56> { let Uses = [IX]; let Defs = [FLAGS]; }
  def BIT_3_IXd : InstDDCB<"bit 3,", 0x5E> { let Uses = [IX]; let Defs = [FLAGS]; }
  def BIT_4_IXd : InstDDCB<"bit 4,", 0x66> { let Uses = [IX]; let Defs = [FLAGS]; }
  def BIT_5_IXd : InstDDCB<"bit 5,", 0x6E> { let Uses = [IX]; let Defs = [FLAGS]; }
  def BIT_6_IXd : InstDDCB<"bit 6,", 0x76> { let Uses = [IX]; let Defs = [FLAGS]; }
  def BIT_7_IXd : InstDDCB<"bit 7,", 0x7E> { let Uses = [IX]; let Defs = [FLAGS]; }
}

// BIT b,(IY+d) — read memory only
let mayLoad = 1, mayStore = 0, hasSideEffects = 0 in {
  def BIT_0_IYd : InstFDCB<"bit 0,", 0x46> { let Uses = [IY]; let Defs = [FLAGS]; }
  def BIT_1_IYd : InstFDCB<"bit 1,", 0x4E> { let Uses = [IY]; let Defs = [FLAGS]; }
  def BIT_2_IYd : InstFDCB<"bit 2,", 0x56> { let Uses = [IY]; let Defs = [FLAGS]; }
  def BIT_3_IYd : InstFDCB<"bit 3,", 0x5E> { let Uses = [IY]; let Defs = [FLAGS]; }
  def BIT_4_IYd : InstFDCB<"bit 4,", 0x66> { let Uses = [IY]; let Defs = [FLAGS]; }
  def BIT_5_IYd : InstFDCB<"bit 5,", 0x6E> { let Uses = [IY]; let Defs = [FLAGS]; }
  def BIT_6_IYd : InstFDCB<"bit 6,", 0x76> { let Uses = [IY]; let Defs = [FLAGS]; }
  def BIT_7_IYd : InstFDCB<"bit 7,", 0x7E> { let Uses = [IY]; let Defs = [FLAGS]; }
}

// RES b,(IX+d) — read and write memory
let mayLoad = 1, mayStore = 1, hasSideEffects = 0 in {
  def RES_0_IXd : InstDDCB<"res 0,", 0x86> { let Uses = [IX]; }
  def RES_1_IXd : InstDDCB<"res 1,", 0x8E> { let Uses = [IX]; }
  def RES_2_IXd : InstDDCB<"res 2,", 0x96> { let Uses = [IX]; }
  def RES_3_IXd : InstDDCB<"res 3,", 0x9E> { let Uses = [IX]; }
  def RES_4_IXd : InstDDCB<"res 4,", 0xA6> { let Uses = [IX]; }
  def RES_5_IXd : InstDDCB<"res 5,", 0xAE> { let Uses = [IX]; }
  def RES_6_IXd : InstDDCB<"res 6,", 0xB6> { let Uses = [IX]; }
  def RES_7_IXd : InstDDCB<"res 7,", 0xBE> { let Uses = [IX]; }
}

// RES b,(IY+d) — read and write memory
let mayLoad = 1, mayStore = 1, hasSideEffects = 0 in {
  def RES_0_IYd : InstFDCB<"res 0,", 0x86> { let Uses = [IY]; }
  def RES_1_IYd : InstFDCB<"res 1,", 0x8E> { let Uses = [IY]; }
  def RES_2_IYd : InstFDCB<"res 2,", 0x96> { let Uses = [IY]; }
  def RES_3_IYd : InstFDCB<"res 3,", 0x9E> { let Uses = [IY]; }
  def RES_4_IYd : InstFDCB<"res 4,", 0xA6> { let Uses = [IY]; }
  def RES_5_IYd : InstFDCB<"res 5,", 0xAE> { let Uses = [IY]; }
  def RES_6_IYd : InstFDCB<"res 6,", 0xB6> { let Uses = [IY]; }
  def RES_7_IYd : InstFDCB<"res 7,", 0xBE> { let Uses = [IY]; }
}

// SET b,(IX+d) — read and write memory
let mayLoad = 1, mayStore = 1, hasSideEffects = 0 in {
  def SET_0_IXd : InstDDCB<"set 0,", 0xC6> { let Uses = [IX]; }
  def SET_1_IXd : InstDDCB<"set 1,", 0xCE> { let Uses = [IX]; }
  def SET_2_IXd : InstDDCB<"set 2,", 0xD6> { let Uses = [IX]; }
  def SET_3_IXd : InstDDCB<"set 3,", 0xDE> { let Uses = [IX]; }
  def SET_4_IXd : InstDDCB<"set 4,", 0xE6> { let Uses = [IX]; }
  def SET_5_IXd : InstDDCB<"set 5,", 0xEE> { let Uses = [IX]; }
  def SET_6_IXd : InstDDCB<"set 6,", 0xF6> { let Uses = [IX]; }
  def SET_7_IXd : InstDDCB<"set 7,", 0xFE> { let Uses = [IX]; }
}

// SET b,(IY+d) — read and write memory
let mayLoad = 1, mayStore = 1, hasSideEffects = 0 in {
  def SET_0_IYd : InstFDCB<"set 0,", 0xC6> { let Uses = [IY]; }
  def SET_1_IYd : InstFDCB<"set 1,", 0xCE> { let Uses = [IY]; }
  def SET_2_IYd : InstFDCB<"set 2,", 0xD6> { let Uses = [IY]; }
  def SET_3_IYd : InstFDCB<"set 3,", 0xDE> { let Uses = [IY]; }
  def SET_4_IYd : InstFDCB<"set 4,", 0xE6> { let Uses = [IY]; }
  def SET_5_IYd : InstFDCB<"set 5,", 0xEE> { let Uses = [IY]; }
  def SET_6_IYd : InstFDCB<"set 6,", 0xF6> { let Uses = [IY]; }
  def SET_7_IYd : InstFDCB<"set 7,", 0xFE> { let Uses = [IY]; }
}

} // Predicates = [HasZ80]

//===----------------------------------------------------------------------===//
// Z80 Spill Pseudos
// These are expanded in expandPostRAPseudo after physical registers are assigned
//===----------------------------------------------------------------------===//

// Base class for Z80 pseudo instructions
class Z80Pseudo<dag outs, dag ins> : Instruction {
  let Namespace = "Z80";
  dag OutOperandList = outs;
  dag InOperandList = ins;
  let hasSideEffects = false;
  let isPseudo = true;
  let isCodeGenOnly = true;
}

// Spill/Reload pseudo instructions for stack slots (frame index).
// No static Defs: small offsets expand to IX+d (no clobbers), large offsets
// are expanded in eliminateFrameIndex with register scavenging.
def SPILL_GR16 : Z80Pseudo<(outs), (ins GR16:$src, i32imm:$fi, i32imm:$offset)> {
  let mayStore = true;
  let mayLoad = false;
}
def RELOAD_GR16 : Z80Pseudo<(outs GR16:$dst), (ins i32imm:$fi, i32imm:$offset)> {
  let mayLoad = true;
  let mayStore = false;
}

// 16-bit add/sub from frame slot via 8-bit IX-indexed ALU.
// ADD_HL_FI expands to: LD A,L; ADD A,(IX+d); LD L,A; LD A,H; ADC A,(IX+d+1); LD H,A
// SUB_HL_FI expands to: LD A,L; SUB (IX+d); LD L,A; LD A,H; SBC A,(IX+d+1); LD H,A
// These avoid allocating a GR16_BCDE register pair, reducing register pressure.
def ADD_HL_FI : Z80Pseudo<(outs), (ins i32imm:$fi)> {
  let Uses = [HL];
  let Defs = [HL, A, FLAGS];
  let mayLoad = true;
  let mayStore = false;
  let Predicates = [HasZ80];
}
def SUB_HL_FI : Z80Pseudo<(outs), (ins i32imm:$fi)> {
  let Uses = [HL];
  let Defs = [HL, A, FLAGS];
  let mayLoad = true;
  let mayStore = false;
  let Predicates = [HasZ80];
}
def SPILL_GR8 : Z80Pseudo<(outs), (ins GR8:$src, i32imm:$fi, i32imm:$offset)> {
  let mayStore = true;
  let mayLoad = false;
}
def SPILL_IMM8 : Z80Pseudo<(outs), (ins i8imm:$val, i32imm:$fi, i32imm:$offset)> {
  let mayStore = true;
  let mayLoad = false;
}
def RELOAD_GR8 : Z80Pseudo<(outs GR8:$dst), (ins i32imm:$fi, i32imm:$offset)> {
  let mayLoad = true;
  let mayStore = false;
}

// LEA_IX_FI: Compute the effective address of a frame index into a GR16.
// Expanded in eliminateFrameIndex using register scavenging to find
// available scratch registers. No static Defs — temporaries are managed
// dynamically via the RegScavenger (PUSH/POP if live).
def LEA_IX_FI : Z80Pseudo<(outs GR16:$dst), (ins i32imm:$fi)> {
  let mayLoad = false;
  let mayStore = false;
}

// LD_r8_n: Load 8-bit immediate into any GR8 register.
// Expanded in expandPostRAPseudo to the concrete LD_r_n opcode.
def LD_r8_n : Z80Pseudo<(outs GR8:$dst), (ins i8imm:$imm)> {
  let mayLoad = false;
  let mayStore = false;
  let isAsCheapAsAMove = true;
  let isMoveImm = true;
  let isReMaterializable = true;
}

// LD_r16_nn: Load 16-bit immediate into any GR16 register.
// Expanded in expandPostRAPseudo to the concrete LD_rr_nn opcode.
def LD_r16_nn : Z80Pseudo<(outs GR16:$dst), (ins i16imm:$imm)> {
  let mayLoad = false;
  let mayStore = false;
  let isAsCheapAsAMove = true;
  let isMoveImm = true;
  let isReMaterializable = true;
}

// INC/DEC 16-bit pseudo instructions.
// Expanded in expandPostRAPseudo to the concrete INC/DEC opcode.
def INC16 : Z80Pseudo<(outs GR16:$dst), (ins GR16:$src)> {
  let Constraints = "$dst = $src";
  let mayLoad = false;
  let mayStore = false;
}
def DEC16 : Z80Pseudo<(outs GR16:$dst), (ins GR16:$src)> {
  let Constraints = "$dst = $src";
  let mayLoad = false;
  let mayStore = false;
}

// SUB_HL_xx: 16-bit subtraction pseudo instructions.
// Generalized 16-bit arithmetic pseudo instructions.
// The second operand (rr) is a GR16_BCDE virtual register — the register
// allocator is free to assign BC or DE.  expandPostRAPseudo inspects the
// physical register and emits the correct real instruction.
//
// HL is always the implicit first operand (hardware constraint: ADD/ADC/SBC
// HL,rr only supports HL as destination).

let isPseudo = true, isCodeGenOnly = true in {

// ADD HL,rr (plain addition, no carry capture)
def ADD_HL_rr : Z80Inst<"add_hl_rr"> {
  let InOperandList = (ins GR16_BCDE:$rhs);
  let OutOperandList = (outs);
  let Uses = [HL];
  let Defs = [HL, FLAGS];
}

// AND A; SBC HL,rr (subtraction without borrow)
def SUB_HL_rr : Z80Inst<"sub_hl_rr"> {
  let InOperandList = (ins GR16_BCDE:$rhs);
  let OutOperandList = (outs);
  let Uses = [HL];
  let Defs = [HL, A, FLAGS];
}

// AND A; ADC HL,rr (signed addition — sets P/V for overflow detection)
def SADD_HL_rr : Z80Inst<"sadd_hl_rr"> {
  let InOperandList = (ins GR16_BCDE:$rhs);
  let OutOperandList = (outs);
  let Uses = [HL];
  let Defs = [HL, A, FLAGS];
}

// ADD HL,rr; SBC A,A; AND 1 — carry out in A
def ADD_HL_rr_CO : Z80Inst<"add_hl_rr_co"> {
  let InOperandList = (ins GR16_BCDE:$rhs);
  let OutOperandList = (outs);
  let Uses = [HL];
  let Defs = [HL, A, FLAGS];
}

// AND A; SBC HL,rr; SBC A,A; AND 1 — borrow out in A
def SUB_HL_rr_BO : Z80Inst<"sub_hl_rr_bo"> {
  let InOperandList = (ins GR16_BCDE:$rhs);
  let OutOperandList = (outs);
  let Uses = [HL];
  let Defs = [HL, A, FLAGS];
}

// LD A,carry; RRCA; ADC HL,rr; SBC A,A; AND 1
def ADC_HL_rr_CIO : Z80Inst<"adc_hl_rr_cio"> {
  let InOperandList = (ins GR16_BCDE:$rhs, GR8:$carry_in);
  let OutOperandList = (outs);
  let Uses = [HL];
  let Defs = [HL, A, FLAGS];
}

// LD A,borrow; RRCA; SBC HL,rr; SBC A,A; AND 1
def SBC_HL_rr_BIO : Z80Inst<"sbc_hl_rr_bio"> {
  let InOperandList = (ins GR16_BCDE:$rhs, GR8:$borrow_in);
  let OutOperandList = (outs);
  let Uses = [HL];
  let Defs = [HL, A, FLAGS];
}

// Generalized 8-bit ALU pseudo instructions.
// The second operand is a GR8 virtual register — the register allocator is
// free to assign any of {A,B,C,D,E,H,L}.  expandPostRAPseudo inspects the
// physical register and emits the correct real instruction (ADD_A_A/ADD_A_B/
// ADD_A_C/etc).
//
// A is always the implicit first operand (hardware constraint: all 8-bit ALU
// ops use A as the accumulator).

def ADD_A_r : Z80Inst<"add_a_r"> {
  let InOperandList = (ins GR8:$rhs);
  let OutOperandList = (outs);
  let Uses = [A];
  let Defs = [A, FLAGS];
}

def SUB_r : Z80Inst<"sub_r"> {
  let InOperandList = (ins GR8:$rhs);
  let OutOperandList = (outs);
  let Uses = [A];
  let Defs = [A, FLAGS];
}

def AND_r : Z80Inst<"and_r"> {
  let InOperandList = (ins GR8:$rhs);
  let OutOperandList = (outs);
  let Uses = [A];
  let Defs = [A, FLAGS];
}

def OR_r : Z80Inst<"or_r"> {
  let InOperandList = (ins GR8:$rhs);
  let OutOperandList = (outs);
  let Uses = [A];
  let Defs = [A, FLAGS];
}

def XOR_r : Z80Inst<"xor_r"> {
  let InOperandList = (ins GR8:$rhs);
  let OutOperandList = (outs);
  let Uses = [A];
  let Defs = [A, FLAGS];
}

def CP_r : Z80Inst<"cp_r"> {
  let InOperandList = (ins GR8:$rhs);
  let OutOperandList = (outs);
  let Uses = [A];
  let Defs = [FLAGS];
}

} // isPseudo, isCodeGenOnly

// CAPTURE_PV: Read the P/V flag (bit 2 of F register) into A as 0 or 1.
// Expanded in expandPostRAPseudo to: PUSH AF; POP HL; LD A,L; RRCA; RRCA; AND 1
// Must be atomic (single pseudo) because the RA must not insert stack-modifying
// instructions between the PUSH AF and POP HL.
let isPseudo = true, isCodeGenOnly = true in {
def CAPTURE_PV : Z80Inst<"capture_pv"> {
  let InOperandList = (ins);
  let OutOperandList = (outs);
  let Uses = [FLAGS];
  let Defs = [A, HL, FLAGS];
}
}

// Zero extend 8-bit to 16-bit.
// Expanded in expandPostRAPseudo: LD lo,src; LD hi,0
def ZEXT_GR8_GR16 : Z80Pseudo<(outs GR16:$dst), (ins GR8:$src)> {
  let mayLoad = false;
  let mayStore = false;
}

// Sign extend 8-bit to 16-bit.
// Expanded in expandPostRAPseudo: LD A,src; LD lo,A; RLCA; SBC A,A; LD hi,A
def SEXT_GR8_GR16 : Z80Pseudo<(outs GR16:$dst), (ins GR8:$src)> {
  let Defs = [A, FLAGS];
  let mayLoad = false;
  let mayStore = false;
}

// 16-bit unsigned comparison pseudo-instructions using 8-bit SUB/SBC chain.
// LD A,lhs_lo; SUB rhs_lo; LD A,lhs_hi; SBC A,rhs_hi
// Carry flag = unsigned less-than. Does NOT clobber HL/DE (unlike SBC HL,DE).
// Both operands can be any GR16 (no GR16_BCDE restriction).

// CMP16_FLAGS: sets carry flag for unsigned comparison. Used for fused branch.
def CMP16_FLAGS : Z80Pseudo<(outs), (ins GR16:$lhs, GR16:$rhs)> {
  let Defs = [A, FLAGS];
  let mayLoad = false;
  let mayStore = false;
}

// CMP16_ULT: materializes unsigned less-than as 0/1 in A.
// Appends SBC A,A; AND 1 after the SUB/SBC chain.
def CMP16_ULT : Z80Pseudo<(outs), (ins GR16:$lhs, GR16:$rhs)> {
  let Defs = [A, FLAGS];
  let mayLoad = false;
  let mayStore = false;
}

// CMP16_SBC_FLAGS: carry-chain continuation for multi-word comparison.
// LD A,lhs_lo; SBC A,rhs_lo; LD A,lhs_hi; SBC A,rhs_hi
// Uses carry-in from previous CMP16_FLAGS or CMP16_SBC_FLAGS.
// Uses = [FLAGS] ensures regalloc won't insert flag-clobbering ops in between.
def CMP16_SBC_FLAGS : Z80Pseudo<(outs), (ins GR16:$lhs, GR16:$rhs)> {
  let Defs = [A, FLAGS];
  let Uses = [FLAGS];
  let mayLoad = false;
  let mayStore = false;
}

// 16-bit XOR-based equality comparison pseudo-instructions.
// These compare two 16-bit registers using byte-level XOR and produce 0/1 in A.
// Expanded in expandPostRAPseudo after register allocation, when physical
// registers are known and sub-register extraction is safe.
// Uses A, B as scratch. Does NOT clobber HL/DE (unlike SBC HL,DE).
def XOR_CMP_EQ16 : Z80Pseudo<(outs), (ins GR16:$lhs, GR16:$rhs)> {
  let Defs = [A, B, FLAGS];
  let mayLoad = false;
  let mayStore = false;
}
def XOR_CMP_NE16 : Z80Pseudo<(outs), (ins GR16:$lhs, GR16:$rhs)> {
  let Defs = [A, B, FLAGS];
  let mayLoad = false;
  let mayStore = false;
}

// 16-bit XOR-based equality comparison — sets Z flag directly.
// XORs both bytes, ORs results: Z=1 if equal, Z=0 if not.
// Same as SM83_CMP_Z16 but available on Z80.
// Used in fused compare-and-branch to avoid 0/1 normalize overhead.
// 6 bytes: LD A,hi; XOR hi'; LD B,A; LD A,lo; XOR lo'; OR B
def XOR_CMP_Z16 : Z80Pseudo<(outs), (ins GR16:$lhs, GR16:$rhs)> {
  let Defs = [A, B, FLAGS];
  let mayLoad = false;
  let mayStore = false;
}

// 16-bit shift right pseudo-instructions.
// Expanded in expandPostRAPseudo to SRL/SRA hi + RR lo.
def LSHR16 : Z80Pseudo<(outs GR16:$dst), (ins GR16:$src)> {
  let Constraints = "$dst = $src";
  let Defs = [FLAGS];
  let mayLoad = false;
  let mayStore = false;
}
def ASHR16 : Z80Pseudo<(outs GR16:$dst), (ins GR16:$src)> {
  let Constraints = "$dst = $src";
  let Defs = [FLAGS];
  let mayLoad = false;
  let mayStore = false;
}

// 16-bit sign extension: result = 0x0000 (positive) or 0xFFFF (negative)
// Expanded to: LD A,src_hi; ADD A,A; SBC A,A; LD dst_lo,A; LD dst_hi,A
// Used for G_ASHR by type_size-1 (e.g., ASHR i16, 15 for sext i16->i32)
def SEXT16 : Z80Pseudo<(outs GR16:$dst), (ins GR16:$src)> {
  let Defs = [A, FLAGS];
  let mayLoad = false;
  let mayStore = false;
}

// Variable shift pseudos - expanded to DJNZ loops in expandPostRAPseudo.
// Pattern: INC B; DEC B; JR Z,done; .loop: <shift>; DJNZ .loop; .done:
// 8-bit: value in A, count in B, result in A. B is decremented to 0.
let mayLoad = false, mayStore = false in {
def SHL8_VAR : Z80Pseudo<(outs), (ins)> {
  let Uses = [A, B];
  let Defs = [A, B, FLAGS];
}
def LSHR8_VAR : Z80Pseudo<(outs), (ins)> {
  let Uses = [A, B];
  let Defs = [A, B, FLAGS];
}
def ASHR8_VAR : Z80Pseudo<(outs), (ins)> {
  let Uses = [A, B];
  let Defs = [A, B, FLAGS];
}
def ROTL8_VAR : Z80Pseudo<(outs), (ins)> {
  let Uses = [A, B];
  let Defs = [A, B, FLAGS];
}
def ROTR8_VAR : Z80Pseudo<(outs), (ins)> {
  let Uses = [A, B];
  let Defs = [A, B, FLAGS];
}
// 16-bit: value in HL, count in B, result in HL. B is decremented to 0.
def SHL16_VAR : Z80Pseudo<(outs), (ins)> {
  let Uses = [HL, B];
  let Defs = [HL, B, FLAGS];
}
def LSHR16_VAR : Z80Pseudo<(outs), (ins)> {
  let Uses = [HL, B];
  let Defs = [HL, B, FLAGS];
}
def ASHR16_VAR : Z80Pseudo<(outs), (ins)> {
  let Uses = [HL, B];
  let Defs = [HL, B, FLAGS];
}
// 8-bit multiply pseudo - expanded to DJNZ shift-add loop.
// Input: A = multiplier, E = multiplicand. Output: A = result (low 8 bits).
// Loop body: ADD A,A; RL D; JR NC,skip; ADD A,E; DJNZ loop
def MUL8 : Z80Pseudo<(outs), (ins)> {
  let Uses = [A, E];
  let Defs = [A, B, D, FLAGS];
}
// 8-bit unsigned division pseudo - expanded to DJNZ restoring division loop.
// Input: A = dividend, E = divisor. Output: A = quotient.
// After loop: D = quotient, A = remainder; UDIV8 returns D, UMOD8 returns A.
def UDIV8 : Z80Pseudo<(outs), (ins)> {
  let Uses = [A, E];
  let Defs = [A, B, D, FLAGS];
}
def UMOD8 : Z80Pseudo<(outs), (ins)> {
  let Uses = [A, E];
  let Defs = [A, B, D, FLAGS];
}
// 8-bit signed division pseudo - sign-handles operands then restoring division.
// Input: A = dividend, E = divisor. Output: A = quotient (SDIV8) or remainder (SMOD8).
def SDIV8 : Z80Pseudo<(outs), (ins)> {
  let Uses = [A, E];
  let Defs = [A, B, C, D, E, FLAGS];
}
def SMOD8 : Z80Pseudo<(outs), (ins)> {
  let Uses = [A, E];
  let Defs = [A, B, C, D, E, FLAGS];
}
// 16-bit multiply pseudo (for +inline-i16-runtime mode).
// Input: HL = multiplicand, DE = multiplier. Output: DE = result.
// Expanded to 16-iteration shift-add loop in Z80ExpandPseudo.
def MUL16 : Z80Pseudo<(outs), (ins)> {
  let Uses = [HL, DE];
  let Defs = [A, B, C, DE, HL, FLAGS];
}
// 16-bit unsigned division pseudo (for +inline-i16-runtime mode).
// Input: HL = dividend, DE = divisor. Output: DE = quotient.
def UDIV16 : Z80Pseudo<(outs), (ins)> {
  let Uses = [HL, DE];
  let Defs = [A, B, C, DE, HL, FLAGS];
}
// 16-bit unsigned modulo pseudo. Output: DE = remainder.
def UMOD16 : Z80Pseudo<(outs), (ins)> {
  let Uses = [HL, DE];
  let Defs = [A, B, C, DE, HL, FLAGS];
}
// 16-bit signed division pseudo. Output: DE = quotient.
def SDIV16 : Z80Pseudo<(outs), (ins)> {
  let Uses = [HL, DE];
  let Defs = [A, B, C, DE, HL, FLAGS];
}
// 16-bit signed modulo pseudo. Output: DE = remainder.
def SMOD16 : Z80Pseudo<(outs), (ins)> {
  let Uses = [HL, DE];
  let Defs = [A, B, C, DE, HL, FLAGS];
}
// 8-bit unsigned saturating add pseudo.
// ADD A,src; JR NC,.done; LD A,#0xFF; .done:
def UADDSAT8 : Z80Pseudo<(outs), (ins GR8:$src)> {
  let Uses = [A];
  let Defs = [A, FLAGS];
}
// 8-bit unsigned saturating subtract pseudo.
// SUB src; JR NC,.done; XOR A; .done:
def USUBSAT8 : Z80Pseudo<(outs), (ins GR8:$src)> {
  let Uses = [A];
  let Defs = [A, FLAGS];
}
// 8-bit signed saturating add pseudo (Z80 only — uses P/V overflow flag).
// ADD A,src; JP PO,.done; RLCA; SBC A,A; XOR #0x80; .done:
def SADDSAT8 : Z80Pseudo<(outs), (ins GR8:$src)> {
  let Uses = [A];
  let Defs = [A, FLAGS];
}
// 8-bit signed saturating subtract pseudo (Z80 only — uses P/V overflow flag).
// SUB src; JP PO,.done; RLCA; SBC A,A; XOR #0x80; .done:
def SSUBSAT8 : Z80Pseudo<(outs), (ins GR8:$src)> {
  let Uses = [A];
  let Defs = [A, FLAGS];
}
} // mayLoad = false, mayStore = false

// CALL_HL - SM83 indirect call through HL register.
// Pseudo-instruction that expands to "call __call_hl" in expandPostRAPseudo.
// The runtime __call_hl trampoline is just "jp (hl)" (1 byte).
// No register shift needed: SM83 args are in DE/BC, so HL is free for
// the function pointer.
let isPseudo = true in
def CALL_HL : Z80Inst<"call\t__call_hl">, InstCall {
  let InOperandList = (ins);
  let Uses = [HL];
  let Defs = [A, BC, DE, HL, FLAGS];
}

// Call frame setup/destroy pseudos for stack argument passing.
// These bracket call sequences where arguments are pushed onto the stack.
// ADJCALLSTACKDOWN is erased (PUSHes adjust SP), ADJCALLSTACKUP emits
// SP cleanup code in eliminateCallFramePseudoInstr.
let Defs = [SP], Uses = [SP], hasSideEffects = true,
    mayLoad = false, mayStore = false in {
def ADJCALLSTACKDOWN : Z80Pseudo<(outs), (ins i16imm:$bytes, i16imm:$prior)>;
}
// ADJCALLSTACKUP may clobber HL (via LD HL,N; ADD HL,SP; LD SP,HL for large
// amounts) and A (via POP AF for small amounts).
let Defs = [SP, HL, A], Uses = [SP], hasSideEffects = true,
    mayLoad = false, mayStore = false in {
def ADJCALLSTACKUP : Z80Pseudo<(outs), (ins i16imm:$bytes, i16imm:$prior)>;
}

// Callee-cleanup return: pops return address, adjusts SP by $bytes, then
// returns via indirect jump. Used by __sdcccall(1) callee-cleanup and
// future __z88dk_callee convention.
let isPseudo = true, isReturn = true, isTerminator = true, isBarrier = true,
    Defs = [SP, HL, BC], Uses = [SP], hasSideEffects = true,
    mayLoad = false, mayStore = false in {
def RET_CLEANUP : Z80Pseudo<(outs), (ins i16imm:$bytes)>;
}

// SM83-only instructions and pseudos
include "SM83InstrInfo.td"

// GlobalISel pseudo-instructions for code generation
include "Z80InstrGISel.td"
