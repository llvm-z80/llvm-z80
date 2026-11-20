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

let Predicates = [IsZ80] in {

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

// LD A,I / LD A,R / LD I,A / LD R,A
def LD_A_I : InstED<"ld a,i", 0x57> { let Uses = [I]; let Defs = [A, F]; }
def LD_A_R : InstED<"ld a,r", 0x5F> { let Uses = [R]; let Defs = [A, F]; }
def LD_I_A : InstED<"ld i,a", 0x47> { let Uses = [A]; let Defs = [I]; }
def LD_R_A : InstED<"ld r,a", 0x4F> { let Uses = [A]; let Defs = [R]; }

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
def LD_BC_nnind : InstEDnn<"ld bc,", 0x4B, DirectAt2Mode> { let Defs = [BC]; }
def LD_DE_nnind : InstEDnn<"ld de,", 0x5B, DirectAt2Mode> { let Defs = [DE]; }
def LD_SP_nnind : InstEDnn<"ld sp,", 0x7B, DirectAt2Mode> { let Defs = [SP]; }
}

let mayLoad = false, mayStore = true, hasSideEffects = false in {
def LD_nnind_BC : InstEDnn<"ld", 0x43, DirectAt2Mode> { let AsmString = "ld ( $imm ),bc"; let Uses = [BC]; }
def LD_nnind_DE : InstEDnn<"ld", 0x53, DirectAt2Mode> { let AsmString = "ld ( $imm ),de"; let Uses = [DE]; }
def LD_nnind_SP : InstEDnn<"ld", 0x73, DirectAt2Mode> { let AsmString = "ld ( $imm ),sp"; let Uses = [SP]; }
}

//===----------------------------------------------------------------------===//
//  Exchange Group
//===----------------------------------------------------------------------===//

let mayLoad = 0, mayStore = 0, hasSideEffects = 0 in {
def EX_DE_HL : Inst8<"ex de,hl", 0xEB> { let Uses = [DE, HL]; let Defs = [DE, HL]; }
def EX_AF_AF : Inst8<"ex af,af'", 0x08> { let Uses = [AF, AFp]; let Defs = [AF, AFp]; }
def EXX : Inst8<"exx", 0xD9> {
  let Uses = [BC, DE, HL, BCp, DEp, HLp];
  let Defs = [BC, DE, HL, BCp, DEp, HLp];
}
}
// sdasz80 also accepts ex af,af without the apostrophe.
def : InstAlias<"ex af,af", (EX_AF_AF), 0>;
let mayLoad = true, mayStore = true, hasSideEffects = false in
def EX_SPind_HL : Inst8<"ex (sp),hl", 0xE3> { let Uses = [SP, HL]; let Defs = [HL]; }

//===----------------------------------------------------------------------===//
//  Block Transfer and Search Group (ED prefixed)
//===----------------------------------------------------------------------===//

// Block transfer: read (HL), write (DE), decrement BC, inc/dec HL and DE
let mayLoad = true, mayStore = true in {
def LDI  : InstED<"ldi",  0xA0> { let Uses = [HL, DE, BC]; let Defs = [HL, DE, BC, F]; }
def LDIR : InstED<"ldir", 0xB0> { let Uses = [HL, DE, BC]; let Defs = [HL, DE, BC, F]; }
def LDD  : InstED<"ldd",  0xA8> { let Uses = [HL, DE, BC]; let Defs = [HL, DE, BC, F]; }
def LDDR : InstED<"lddr", 0xB8> { let Uses = [HL, DE, BC]; let Defs = [HL, DE, BC, F]; }
}

// Block compare: read (HL), compare with A, decrement BC, inc/dec HL
let mayLoad = true, mayStore = false in {
def CPI  : InstED<"cpi",  0xA1> { let Uses = [HL, A, BC]; let Defs = [HL, BC, F]; }
def CPIR : InstED<"cpir", 0xB1> { let Uses = [HL, A, BC]; let Defs = [HL, BC, F]; }
def CPD  : InstED<"cpd",  0xA9> { let Uses = [HL, A, BC]; let Defs = [HL, BC, F]; }
def CPDR : InstED<"cpdr", 0xB9> { let Uses = [HL, A, BC]; let Defs = [HL, BC, F]; }
}

//===----------------------------------------------------------------------===//
//  General-Purpose Arithmetic and CPU Control (Z80-only)
//===----------------------------------------------------------------------===//

let mayLoad = 0, mayStore = 0, hasSideEffects = 0 in {
def NEG : InstED<"neg", 0x44> { let Uses = [A]; let Defs = [A, F]; }
}

// IM instructions
def IM_0 : InstED<"im 0", 0x46>;
def IM_1 : InstED<"im 1", 0x56>;
def IM_2 : InstED<"im 2", 0x5E>;

//===----------------------------------------------------------------------===//
//  16-bit Arithmetic Group (Z80-only: ED prefixed)
//===----------------------------------------------------------------------===//

// ADC HL,rr and SBC HL,rr
class ADCSBCHLrr<string mnemonic, bits<4> lo>
    : Z80Inst16<mnemonic # " hl, $src"> {
  bits<2> src;
  let Inst{7-0} = 0xED;
  let Inst{15-14} = 0b01;
  let Inst{13-12} = src;
  let Inst{11-8} = lo;
  let OutOperandList = (outs);
  let InOperandList = (ins GR16:$src);
}

let mayLoad = 0, mayStore = 0, hasSideEffects = 0 in {
def ADC_HL_rr : ADCSBCHLrr<"adc", 0b1010> { let Uses = [HL, F]; let Defs = [HL, F]; }
}
def ADC_HL_SP : InstED<"adc hl,sp", 0x7A> { let Uses = [HL, SP, F]; let Defs = [HL, F]; }

let mayLoad = 0, mayStore = 0, hasSideEffects = 0 in {
def SBC_HL_rr : ADCSBCHLrr<"sbc", 0b0010> { let Uses = [HL, F]; let Defs = [HL, F]; }
}
def SBC_HL_SP : InstED<"sbc hl,sp", 0x72> { let Uses = [HL, SP, F]; let Defs = [HL, F]; }

//===----------------------------------------------------------------------===//
//  Rotate and Shift Group (Z80-only: ED prefixed)
//===----------------------------------------------------------------------===//

// RLD / RRD (ED prefixed) - set flags
let mayLoad = true, mayStore = true, hasSideEffects = false in {
def RLD : InstED<"rld", 0x6F> { let Uses = [HL, A]; let Defs = [A, F]; }
def RRD : InstED<"rrd", 0x67> { let Uses = [HL, A]; let Defs = [A, F]; }
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
let isBranch = true, isTerminator = true in
def DJNZ_e : Inst16<"djnz", 0x10, RelativeMode>;

//===----------------------------------------------------------------------===//
//  Call and Return Group (Z80-only)
//===----------------------------------------------------------------------===//

// CALL_IY - Indirect call through IY register.
// Pseudo-instruction that expands to "call __call_iy" in expandPostRAPseudo.
// The runtime __call_iy trampoline does "jp (iy)".
// IY holds the function pointer; HL/DE are free for arguments.
def CALL_IY : Z80Pseudo<(outs), (ins)> {
  let isCall = true;
  let Uses = [IY];
  let Defs = [A, BC, DE, HL, IY, F];
  let mayLoad = true;
  let mayStore = true;
  let hasSideEffects = true;
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
let isReturn = true, isTerminator = true, isBarrier = true in {
def RETI : InstED<"reti", 0x4D>;
def RETN : InstED<"retn", 0x45>;
}

//===----------------------------------------------------------------------===//
//  Input and Output Group
//===----------------------------------------------------------------------===//

// IN A,(n)
def IN_A_n : Inst16<"in a,", 0xDB, PortMode> { let Defs = [A]; }

// OUT (n),A
def OUT_n_A : Inst16<"out", 0xD3, PortMode> {
  let AsmString = "out ( $imm ), a";
  let Uses = [A];
}

// IN r,(C)
let Uses = [C], Defs = [F], hasSideEffects = true, mayLoad = false,
    mayStore = false in
def IN_r_C : Z80Inst16<"in $dst, (c)"> {
  bits<3> dst;
  let Inst{7-0} = 0xED;
  let Inst{15-14} = 0b01;
  let Inst{13-11} = dst;
  let Inst{10-8} = 0b000;
  let OutOperandList = (outs GR8:$dst);
  let InOperandList = (ins);
}

// OUT (C),r
let Uses = [C], hasSideEffects = true, mayLoad = false, mayStore = false in
def OUT_C_r : Z80Inst16<"out (c), $src"> {
  bits<3> src;
  let Inst{7-0} = 0xED;
  let Inst{15-14} = 0b01;
  let Inst{13-11} = src;
  let Inst{10-8} = 0b001;
  let InOperandList = (ins GR8:$src);
}

// Block I/O: port (C), byte at (HL), count in B
let Uses = [B, C, HL], Defs = [B, HL, F] in {
def INI  : InstED<"ini",  0xA2>;
def INIR : InstED<"inir", 0xB2>;
def IND  : InstED<"ind",  0xAA>;
def INDR : InstED<"indr", 0xBA>;

def OUTI : InstED<"outi", 0xA3>;
def OTIR : InstED<"otir", 0xB3>;
def OUTD : InstED<"outd", 0xAB>;
def OTDR : InstED<"otdr", 0xBB>;
}

} // Predicates = [IsZ80]

//===----------------------------------------------------------------------===//
//  IX/IY Instructions (DD/FD prefixed) - Selected examples
//===----------------------------------------------------------------------===//

let Predicates = [IsZ80] in {

// LD IX,nn / LD IY,nn
let mayLoad = 0, mayStore = 0, hasSideEffects = 0 in {
def LD_IX_nn : InstDDnn<"ld ix,", 0x21, Imm16At2Mode> { let Defs = [IX]; }
def LD_IY_nn : InstFDnn<"ld iy,", 0x21, Imm16At2Mode> { let Defs = [IY]; }
}

// LD IX,(nn) / LD IY,(nn)
let mayLoad = 1, mayStore = 0, hasSideEffects = 0 in {
def LD_IX_nnind : InstDDnn<"ld ix,", 0x2A, DirectAt2Mode> { let Defs = [IX]; }
def LD_IY_nnind : InstFDnn<"ld iy,", 0x2A, DirectAt2Mode> { let Defs = [IY]; }
}

// LD (nn),IX / LD (nn),IY
let mayLoad = 0, mayStore = 1, hasSideEffects = 0 in {
def LD_nnind_IX : InstDDnn<"ld", 0x22, DirectAt2Mode> {
  let AsmString = "ld ( $imm ),ix";
  let Uses = [IX];
}
def LD_nnind_IY : InstFDnn<"ld", 0x22, DirectAt2Mode> {
  let AsmString = "ld ( $imm ),iy";
  let Uses = [IY];
}
}

// PUSH IX / PUSH IY
def PUSH_IX : InstDD<"push ix", 0xE5> { let Uses = [IX]; }
def PUSH_IY : InstFD<"push iy", 0xE5> { let Uses = [IY]; }

// POP IX / POP IY
def POP_IX : InstDD<"pop ix", 0xE1> { let Defs = [IX]; }
def POP_IY : InstFD<"pop iy", 0xE1> { let Defs = [IY]; }

// JP (IX) / JP (IY)
let isBranch = true, isTerminator = true, isBarrier = true,
    isIndirectBranch = true in {
def JP_IXind : InstDD<"jp (ix)", 0xE9> { let Uses = [IX]; }
def JP_IYind : InstFD<"jp (iy)", 0xE9> { let Uses = [IY]; }
}

// LD SP,IX / LD SP,IY
def LD_SP_IX : InstDD<"ld sp,ix", 0xF9> { let Uses = [IX]; let Defs = [SP]; }
def LD_SP_IY : InstFD<"ld sp,iy", 0xF9> { let Uses = [IY]; let Defs = [SP]; }

// INC IX / INC IY / DEC IX / DEC IY
let mayLoad = 0, mayStore = 0, hasSideEffects = 0 in {
def INC_IX : InstDD<"inc ix", 0x23> { let Uses = [IX]; let Defs = [IX]; }
def INC_IY : InstFD<"inc iy", 0x23> { let Uses = [IY]; let Defs = [IY]; }
def DEC_IX : InstDD<"dec ix", 0x2B> { let Uses = [IX]; let Defs = [IX]; }
def DEC_IY : InstFD<"dec iy", 0x2B> { let Uses = [IY]; let Defs = [IY]; }

// ADD IX,pp / ADD IY,rr (sets carry, H)
def ADD_IX_BC : InstDD<"add ix,bc", 0x09> { let Uses = [IX, BC]; let Defs = [IX, F]; }
def ADD_IX_DE : InstDD<"add ix,de", 0x19> { let Uses = [IX, DE]; let Defs = [IX, F]; }
def ADD_IX_IX : InstDD<"add ix,ix", 0x29> { let Uses = [IX];     let Defs = [IX, F]; }
}
def ADD_IX_SP : InstDD<"add ix,sp", 0x39> { let Uses = [IX, SP]; let Defs = [IX, F]; }

let mayLoad = 0, mayStore = 0, hasSideEffects = 0 in {
def ADD_IY_BC : InstFD<"add iy,bc", 0x09> { let Uses = [IY, BC]; let Defs = [IY, F]; }
def ADD_IY_DE : InstFD<"add iy,de", 0x19> { let Uses = [IY, DE]; let Defs = [IY, F]; }
def ADD_IY_IY : InstFD<"add iy,iy", 0x29> { let Uses = [IY];     let Defs = [IY, F]; }
}
def ADD_IY_SP : InstFD<"add iy,sp", 0x39> { let Uses = [IY, SP]; let Defs = [IY, F]; }

// EX (SP),IX / EX (SP),IY
let mayLoad = true, mayStore = true, hasSideEffects = false in {
def EX_SPind_IX : InstDD<"ex (sp),ix", 0xE3> { let Uses = [SP, IX]; let Defs = [IX]; }
def EX_SPind_IY : InstFD<"ex (sp),iy", 0xE3> { let Uses = [SP, IY]; let Defs = [IY]; }
}

//===----------------------------------------------------------------------===//
// Indexed addressing: LD r,(IX+d) / LD r,(IY+d)
// Opcode: DD [46|4E|56|5E|66|6E|7E] d for IX
// Opcode: FD [46|4E|56|5E|66|6E|7E] d for IY
//===----------------------------------------------------------------------===//

// LD r,(IX+d) and LD r,(IY+d)
class LDrIdxd<string mnemonic, bits<8> prefix, string idx>
    : Z80Inst24<mnemonic # " $dst, ( " # idx # " $imm )"> {
  bits<3> dst;
  bits<8> imm;
  let Inst{7-0} = prefix;
  let Inst{15-14} = 0b01;
  let Inst{13-11} = dst;
  let Inst{10-8} = 0b110;
  let Inst{23-16} = imm;
  let OutOperandList = (outs GR8:$dst);
  let InOperandList = (ins disp8:$imm);
  let mayLoad = 1;
  let mayStore = 0;
  let Uses = !if(!eq(idx, "ix"), [IX], [IY]);
  let hasSideEffects = 0;
}

def LD_r_IXd : LDrIdxd<"ld", 0xDD, "ix">;
def LD_r_IYd : LDrIdxd<"ld", 0xFD, "iy">;

//===----------------------------------------------------------------------===//
// Indexed addressing: LD (IX+d),r / LD (IY+d),r
// Opcode: DD [70|71|72|73|74|75|77] d for IX
// Opcode: FD [70|71|72|73|74|75|77] d for IY
//===----------------------------------------------------------------------===//

// LD (IX+d),r and LD (IY+d),r
class LDIdxdr<string mnemonic, bits<8> prefix, string idx>
    : Z80Inst24<mnemonic # " ( " # idx # " $imm ), $src"> {
  bits<3> src;
  bits<8> imm;
  let Inst{7-0} = prefix;
  let Inst{15-14} = 0b01;
  let Inst{13-11} = 0b110;
  let Inst{10-8} = src;
  let Inst{23-16} = imm;
  let OutOperandList = (outs);
  let InOperandList = (ins disp8:$imm, GR8:$src);
  let mayLoad = 0;
  let mayStore = 1;
  let Uses = !if(!eq(idx, "ix"), [IX], [IY]);
  let hasSideEffects = 0;
}

// The load and the store patterns overlap on the byte where both would name
// the memory slot as their register, so the store decodes from its own table.
// That byte is HALT under a DD or FD prefix, which the two-byte definitions
// below catch first anyway.
let DecoderNamespace = "Z80Idx" in {
def LD_IXd_r : LDIdxdr<"ld", 0xDD, "ix">;
def LD_IYd_r : LDIdxdr<"ld", 0xFD, "iy">;
}

// A DD or FD prefix in front of HALT does nothing, so the disassembler decodes
// the pair as HALT and not as an indexed load.
let AsmVariantName = "NonParsable" in {
def HALT_DD : InstDD<"halt", 0x76>;
def HALT_FD : InstFD<"halt", 0x76>;
}

//===----------------------------------------------------------------------===//
// Indexed addressing: LD (IX+d),n / LD (IY+d),n - Store immediate to indexed
// Opcode: DD 36 d n for IX
// Opcode: FD 36 d n for IY
//===----------------------------------------------------------------------===//

let mayLoad = 0, mayStore = 1, hasSideEffects = 0 in {
def LD_IXd_n : InstDDdn<"ld", 0x36> { let Uses = [IX]; }
def LD_IYd_n : InstFDdn<"ld", 0x36> { let Uses = [IY]; }
}

//===----------------------------------------------------------------------===//
//  IX-indexed ALU instructions (DD prefix)
//===----------------------------------------------------------------------===//

let mayLoad = 1, mayStore = 0, hasSideEffects = 0 in {
  def ADD_A_IXd : InstDDd<"add a,", 0x86> {
    let Uses = [A, IX]; let Defs = [A, F];
  }
  def ADC_A_IXd : InstDDd<"adc a,", 0x8E> {
    let Uses = [A, IX, F]; let Defs = [A, F];
  }
  def SUB_IXd : InstDDd<"sub", 0x96> {
    let AsmString = "sub ( ix $imm )";
    let Uses = [A, IX]; let Defs = [A, F];
  }
  def SBC_A_IXd : InstDDd<"sbc a,", 0x9E> {
    let Uses = [A, IX, F]; let Defs = [A, F];
  }
  def AND_IXd : InstDDd<"and", 0xA6> {
    let AsmString = "and ( ix $imm )";
    let Uses = [A, IX]; let Defs = [A, F];
  }
  def XOR_IXd : InstDDd<"xor", 0xAE> {
    let AsmString = "xor ( ix $imm )";
    let Uses = [A, IX]; let Defs = [A, F];
  }
  def OR_IXd : InstDDd<"or", 0xB6> {
    let AsmString = "or ( ix $imm )";
    let Uses = [A, IX]; let Defs = [A, F];
  }
  def CP_IXd : InstDDd<"cp", 0xBE> {
    let AsmString = "cp ( ix $imm )";
    let Uses = [A, IX]; let Defs = [F];
  }
}

//===----------------------------------------------------------------------===//
//  IY-indexed ALU instructions (FD prefix)
//===----------------------------------------------------------------------===//

let mayLoad = 1, mayStore = 0, hasSideEffects = 0 in {
  def ADD_A_IYd : InstFDd<"add a,", 0x86> {
    let Uses = [A, IY]; let Defs = [A, F];
  }
  def ADC_A_IYd : InstFDd<"adc a,", 0x8E> {
    let Uses = [A, IY, F]; let Defs = [A, F];
  }
  def SUB_IYd : InstFDd<"sub", 0x96> {
    let AsmString = "sub ( iy $imm )";
    let Uses = [A, IY]; let Defs = [A, F];
  }
  def SBC_A_IYd : InstFDd<"sbc a,", 0x9E> {
    let Uses = [A, IY, F]; let Defs = [A, F];
  }
  def AND_IYd : InstFDd<"and", 0xA6> {
    let AsmString = "and ( iy $imm )";
    let Uses = [A, IY]; let Defs = [A, F];
  }
  def XOR_IYd : InstFDd<"xor", 0xAE> {
    let AsmString = "xor ( iy $imm )";
    let Uses = [A, IY]; let Defs = [A, F];
  }
  def OR_IYd : InstFDd<"or", 0xB6> {
    let AsmString = "or ( iy $imm )";
    let Uses = [A, IY]; let Defs = [A, F];
  }
  def CP_IYd : InstFDd<"cp", 0xBE> {
    let AsmString = "cp ( iy $imm )";
    let Uses = [A, IY]; let Defs = [F];
  }
}

//===----------------------------------------------------------------------===//
//  IX/IY-indexed INC/DEC (DD/FD prefix)
//===----------------------------------------------------------------------===//

let mayLoad = 1, mayStore = 1, hasSideEffects = 0 in {
  def INC_IXd : InstDDd<"inc", 0x34> {
    let AsmString = "inc ( ix $imm )";
    let Uses = [IX]; let Defs = [F];
  }
  def DEC_IXd : InstDDd<"dec", 0x35> {
    let AsmString = "dec ( ix $imm )";
    let Uses = [IX]; let Defs = [F];
  }
  def INC_IYd : InstFDd<"inc", 0x34> {
    let AsmString = "inc ( iy $imm )";
    let Uses = [IY]; let Defs = [F];
  }
  def DEC_IYd : InstFDd<"dec", 0x35> {
    let AsmString = "dec ( iy $imm )";
    let Uses = [IY]; let Defs = [F];
  }
}

//===----------------------------------------------------------------------===//
//  DDCB/FDCB indexed rotate/shift instructions (4-byte)
//===----------------------------------------------------------------------===//

// Rotate/shift (IX+d) — read and write memory
let mayLoad = 1, mayStore = 1, hasSideEffects = 0 in {
  def RLC_IXd : InstDDCB<"rlc", 0x06> { let Uses = [IX]; let Defs = [F]; }
  def RRC_IXd : InstDDCB<"rrc", 0x0E> { let Uses = [IX]; let Defs = [F]; }
  def RL_IXd  : InstDDCB<"rl",  0x16> { let Uses = [IX, F]; let Defs = [F]; }
  def RR_IXd  : InstDDCB<"rr",  0x1E> { let Uses = [IX, F]; let Defs = [F]; }
  def SLA_IXd : InstDDCB<"sla", 0x26> { let Uses = [IX]; let Defs = [F]; }
  def SRA_IXd : InstDDCB<"sra", 0x2E> { let Uses = [IX]; let Defs = [F]; }
  def SRL_IXd : InstDDCB<"srl", 0x3E> { let Uses = [IX]; let Defs = [F]; }
}

// Rotate/shift (IY+d) — read and write memory
let mayLoad = 1, mayStore = 1, hasSideEffects = 0 in {
  def RLC_IYd : InstFDCB<"rlc", 0x06> { let Uses = [IY]; let Defs = [F]; }
  def RRC_IYd : InstFDCB<"rrc", 0x0E> { let Uses = [IY]; let Defs = [F]; }
  def RL_IYd  : InstFDCB<"rl",  0x16> { let Uses = [IY, F]; let Defs = [F]; }
  def RR_IYd  : InstFDCB<"rr",  0x1E> { let Uses = [IY, F]; let Defs = [F]; }
  def SLA_IYd : InstFDCB<"sla", 0x26> { let Uses = [IY]; let Defs = [F]; }
  def SRA_IYd : InstFDCB<"sra", 0x2E> { let Uses = [IY]; let Defs = [F]; }
  def SRL_IYd : InstFDCB<"srl", 0x3E> { let Uses = [IY]; let Defs = [F]; }
}

//===----------------------------------------------------------------------===//
//  DDCB/FDCB indexed BIT/SET/RES instructions (4-byte)
//===----------------------------------------------------------------------===//

// BIT/RES/SET b,(IX+d)
class BitOpIXd<string mnemonic, bits<2> op>
    : Z80Inst32<mnemonic # " $b, ( ix $imm )"> {
  bits<3> b;
  bits<8> imm;
  let Inst{7-0} = 0xDD;
  let Inst{15-8} = 0xCB;
  let Inst{23-16} = imm;
  let Inst{31-30} = op;
  let Inst{29-27} = b;
  let Inst{26-24} = 0b110;
  let InOperandList = (ins imm3:$b, disp8:$imm);
}

// BIT/RES/SET b,(IY+d)
class BitOpIYd<string mnemonic, bits<2> op>
    : Z80Inst32<mnemonic # " $b, ( iy $imm )"> {
  bits<3> b;
  bits<8> imm;
  let Inst{7-0} = 0xFD;
  let Inst{15-8} = 0xCB;
  let Inst{23-16} = imm;
  let Inst{31-30} = op;
  let Inst{29-27} = b;
  let Inst{26-24} = 0b110;
  let InOperandList = (ins imm3:$b, disp8:$imm);
}

let mayLoad = 1, mayStore = 0, hasSideEffects = 0, Defs = [F] in {
  def BIT_b_IXd : BitOpIXd<"bit", 0b01> { let Uses = [IX]; }
  def BIT_b_IYd : BitOpIYd<"bit", 0b01> { let Uses = [IY]; }
}

let mayLoad = 1, mayStore = 1, hasSideEffects = 0 in {
  def RES_b_IXd : BitOpIXd<"res", 0b10> { let Uses = [IX]; }
  def RES_b_IYd : BitOpIYd<"res", 0b10> { let Uses = [IY]; }
  def SET_b_IXd : BitOpIXd<"set", 0b11> { let Uses = [IX]; }
  def SET_b_IYd : BitOpIYd<"set", 0b11> { let Uses = [IY]; }
}

} // Predicates = [IsZ80]

//===----------------------------------------------------------------------===//
// Z80 Spill Pseudos
// These are expanded in expandPostRAPseudo after physical registers are assigned
//===----------------------------------------------------------------------===//

// The 8-bit ALU with the accumulator as an operand. The real instructions
// name A implicitly, so selecting them directly pins every operand and
// result to A through physical copies. These take the accumulator as a
// virtual register in Ac instead, which leaves the register allocator to
// decide where each value lives, and expand into the real instructions once
// it has put them in A.
multiclass AccALU {
  let Constraints = "$dst = $lhs", Defs = [F],
      mayLoad = false, mayStore = false in {
    def _r : Z80Pseudo<(outs Ac:$dst), (ins Ac:$lhs, GR8:$rhs)>;
    def _n : Z80Pseudo<(outs Ac:$dst), (ins Ac:$lhs, i8imm:$rhs)>;
  }
}
defm ADD_Ac : AccALU;
defm SUB_Ac : AccALU;
defm AND_Ac : AccALU;
defm OR_Ac  : AccALU;
defm XOR_Ac : AccALU;

// Comparisons, which set the flags and leave A as it was: CP, and OR A for
// A against zero.
let Defs = [F], isCompare = true, mayLoad = false, mayStore = false in {
  def CP_Ac_r : Z80Pseudo<(outs), (ins Ac:$lhs, GR8:$rhs)>;
  def CP_Ac_n : Z80Pseudo<(outs), (ins Ac:$lhs, i8imm:$rhs)>;
  def TST_Ac : Z80Pseudo<(outs), (ins Ac:$src)>;
}

// Operations on A alone: ADD A,A doubles it, and CPL, the rotates and SM83's
// SWAP name no other register.
let Constraints = "$dst = $src", Defs = [F],
    mayLoad = false, mayStore = false in {
  def ADD_Ac_Ac : Z80Pseudo<(outs Ac:$dst), (ins Ac:$src)>;
  def CPL_Ac : Z80Pseudo<(outs Ac:$dst), (ins Ac:$src)>;
  def RLCA_Ac : Z80Pseudo<(outs Ac:$dst), (ins Ac:$src)>;
  def RRCA_Ac : Z80Pseudo<(outs Ac:$dst), (ins Ac:$src)>;
  let Predicates = [IsSM83] in
  def SWAP_Ac : Z80Pseudo<(outs Ac:$dst), (ins Ac:$src)>;
}

// SM83's loads and stores of A at an absolute address, the second in the
// top page.
let Predicates = [IsSM83] in {
  let mayLoad = true, mayStore = false in {
    def SM83_LD_Ac_nnind : Z80Pseudo<(outs Ac:$dst), (ins i16imm:$addr)>;
    def SM83_LDH_Ac_nind : Z80Pseudo<(outs Ac:$dst), (ins i8imm:$port)>;
  }
  let mayLoad = false, mayStore = true in {
    def SM83_LD_nnind_Ac : Z80Pseudo<(outs), (ins Ac:$src, i16imm:$addr)>;
    def SM83_LDH_nind_Ac : Z80Pseudo<(outs), (ins Ac:$src, i8imm:$port)>;
  }
}

// SBC A,A: nothing of A survives, so the result is the carry alone, spread
// to 0 or 0xFF.
let Uses = [F], Defs = [F], mayLoad = false, mayStore = false in
def SBC_Ac_Ac : Z80Pseudo<(outs Ac:$dst), (ins)>;

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
  // Allow rematerialization for loads from immutable stack slots (e.g. incoming
  // arguments). isReMaterializableImpl checks isImmutableObjectIndex, so only
  // fixed-stack args are actually rematerialized — spill slot reloads are not.
  let isReMaterializable = true;
}

// Registers that don't fit the GR16 operand — physical IX/IY, and virtual
// registers in the wider Any16 class — spill through a separate pair rather
// than a widened SPILL_GR16: the descriptor's class reaches the register
// allocator through its temporaries, and widening it perturbs allocation.
def SPILL_ANY16 : Z80Pseudo<(outs), (ins Any16:$src, i32imm:$fi, i32imm:$offset)> {
  let mayStore = true;
  let mayLoad = false;
}
def RELOAD_ANY16 : Z80Pseudo<(outs Any16:$dst), (ins i32imm:$fi, i32imm:$offset)> {
  let mayLoad = true;
  let mayStore = false;
  let isReMaterializable = true;
}

// SPILL_PUSH and SPILL_POP are the PUSH and POP of a spill slot replaced by
// Z80PushPopSpills. getSPAdjust returns 0 for this pair.
// Z80InstrInfo::getSpillPushDepth counts this pair for PEI instead.
// expandPostRAPseudo lowers this pair to PUSH and POP.
def SPILL_PUSH : Z80Pseudo<(outs), (ins Any16:$src)> {
  let mayStore = true;
  let mayLoad = false;
}
def SPILL_POP : Z80Pseudo<(outs Any16:$dst), (ins)> {
  let mayLoad = true;
  let mayStore = false;
}

// 8-bit ALU against a frame slot, with the accumulator as an operand as in
// the register forms. $op says which operation, so one pseudo covers them
// all. Selection cannot name the addressing mode yet: whether the frame
// pointer exists and whether the displacement fits are settled in
// eliminateFrameIndex, which picks IX+d, an address in a pair, or unfolds
// back to a reload and a register ALU.
def ALU_Ac_FI : Z80Pseudo<(outs Ac:$dst),
                          (ins Ac:$lhs, i32imm:$op, i32imm:$fi, i32imm:$offset)> {
  let Constraints = "$dst = $lhs";
  let Defs = [F];
  let mayLoad = true;
  let mayStore = false;
  let Predicates = [IsZ80];
}
def SPILL_GR8 : Z80Pseudo<(outs), (ins GR8:$src, i32imm:$fi, i32imm:$offset)> {
  let mayStore = true;
  let mayLoad = false;
}
def SPILL_IMM8 : Z80Pseudo<(outs), (ins i8imm:$val, i32imm:$fi, i32imm:$offset)> {
  let mayStore = true;
  let mayLoad = false;
}
// 16-bit constant store to a frame slot. Storing the two halves as
// immediates keeps the value out of a register pair entirely, which both
// shortens the store and leaves the pair free for allocation.
def SPILL_IMM16 : Z80Pseudo<(outs), (ins i16imm:$val, i32imm:$fi, i32imm:$offset)> {
  let mayStore = true;
  let mayLoad = false;
}
def RELOAD_GR8 : Z80Pseudo<(outs GR8:$dst), (ins i32imm:$fi, i32imm:$offset)> {
  let mayLoad = true;
  let mayStore = false;
  let isReMaterializable = true;
}

// LEA_IX_FI: Compute the effective address of a frame index into a GR16.
// Expanded in eliminateFrameIndex using register scavenging to find
// available scratch registers. No static Defs — temporaries are managed
// dynamically via the RegScavenger (PUSH/POP if live).
def LEA_IX_FI : Z80Pseudo<(outs GR16:$dst), (ins i32imm:$fi)> {
  let mayLoad = false;
  let mayStore = false;
}

// 8-bit indirect load/store via any register pair (BC, DE, or HL).
// Regalloc chooses which pair; expandPostRAPseudo selects the instruction:
//   BC → LD A,(BC) / LD (BC),A
//   DE → LD A,(DE) / LD (DE),A
//   HL → LD A,(HL) / LD (HL),A
// This avoids forcing the address into HL, reducing register pressure.
def LOAD8_IND : Z80Pseudo<(outs Ac:$dst), (ins GR16:$addr)> {
  let mayLoad = true;
  let mayStore = false;
}
def STORE8_IND : Z80Pseudo<(outs), (ins Ac:$src, GR16:$addr)> {
  let mayLoad = false;
  let mayStore = true;
}

// Direct access to an address the linker settles: a global, or a global plus a
// constant. Z80 reaches one in a single instruction per pair, against putting
// the address in a pointer register and reading the two bytes through it. The
// pair is left to the register allocator, since every member of GR16 has a
// direct form for expandPostRAPseudo to pick:
//   HL → LD HL,(nn) / LD (nn),HL     (3 bytes)
//   BC → LD BC,(nn) / LD (nn),BC     (4 bytes, ED prefix)
//   DE → LD DE,(nn) / LD (nn),DE     (4 bytes, ED prefix)
// SM83 has none of these, so it never selects them.
def LOAD16_ABS : Z80Pseudo<(outs GR16:$dst), (ins i16imm:$addr)> {
  let mayLoad = true;
  let mayStore = false;
}
def STORE16_ABS : Z80Pseudo<(outs), (ins i16imm:$addr, GR16:$src)> {
  let mayLoad = false;
  let mayStore = true;
}

// SUB_HL_xx: 16-bit subtraction pseudo instructions.
// Generalized 16-bit arithmetic pseudo instructions.
// The second operand (rr) is a GR16_BCDE virtual register — the register
// allocator is free to assign BC or DE.  expandPostRAPseudo inspects the
// physical register and emits the correct real instruction.
//
// HL is always the implicit first operand (hardware constraint: ADD/ADC/SBC
// HL,rr only supports HL as destination).

let Uses = [HL], Defs = [HL, A, F], mayLoad = false, mayStore = false in {

// AND A; SBC HL,rr (subtraction without borrow)
def SUB_HL_rr : Z80Pseudo<(outs), (ins GR16_BCDE:$rhs)>;

// ADC HL,rr and SBC HL,rr with the carry or borrow in taken from a register,
// restored into the flags right before the instruction that reads it. The
// carry or borrow out is left in the flags.
// LD A,carry; RRCA; ADC HL,rr
def ADC_HL_rr_CI : Z80Pseudo<(outs), (ins GR16_BCDE:$rhs, GR8:$carry_in)>;

// LD A,borrow; RRCA; SBC HL,rr
def SBC_HL_rr_BI : Z80Pseudo<(outs), (ins GR16_BCDE:$rhs, GR8:$borrow_in)>;

}

// HL plus or minus a pair, with the signed overflow in A as 0 or 1. $tmp is
// scratch.
let Uses = [HL], Defs = [HL, F], Constraints = "@earlyclobber $tmp",
    mayLoad = false, mayStore = false in {
  def SADDO16 : Z80Pseudo<(outs Ac:$ov, GR16_BCDE:$tmp), (ins GR16_BCDE:$rhs)>;
  def SSUBO16 : Z80Pseudo<(outs Ac:$ov, GR16_BCDE:$tmp), (ins GR16_BCDE:$rhs)>;
}

// Zero extend 8-bit to 16-bit.
// Expanded in expandPostRAPseudo: LD lo,src; LD hi,0
def ZEXT_GR8_GR16 : Z80Pseudo<(outs GR16:$dst), (ins GR8:$src)> {
  let mayLoad = false;
  let mayStore = false;
}

// A byte shifted into the high half of a pair, the low half zero.
// Expanded in expandPostRAPseudo: LD hi,src; LD lo,0
def SHL8_GR8_GR16 : Z80Pseudo<(outs GR16:$dst), (ins GR8:$src)> {
  let mayLoad = false;
  let mayStore = false;
}

// Sign extend 8-bit to 16-bit.
// Expanded in expandPostRAPseudo: LD A,src; LD lo,A; RLCA; SBC A,A; LD hi,A
def SEXT_GR8_GR16 : Z80Pseudo<(outs GR16:$dst), (ins GR8:$src)> {
  let Defs = [A, F];
  let mayLoad = false;
  let mayStore = false;
}

// 16-bit unsigned comparison pseudo-instructions using 8-bit SUB/SBC chain.
// LD A,lhs_lo; SUB rhs_lo; LD A,lhs_hi; SBC A,rhs_hi
// Carry flag = unsigned less-than. Does NOT clobber HL/DE (unlike SBC HL,DE).
// Both operands can be any GR16 (no GR16_BCDE restriction).

// CMP16_FLAGS: sets carry flag for unsigned comparison. Used for fused branch.
def CMP16_FLAGS : Z80Pseudo<(outs), (ins GR16:$lhs, GR16:$rhs)> {
  let Defs = [A, F];
  let mayLoad = false;
  let mayStore = false;
}

// CMP16_FLAGS against an immediate, which needs no pair for the constant:
// LD A,lhs_lo; SUB lo; LD A,lhs_hi; SBC A,hi.
def CMP16_FLAGS_IMM : Z80Pseudo<(outs), (ins GR16:$lhs, i16imm:$rhs)> {
  let Defs = [A, F];
  let mayLoad = false;
  let mayStore = false;
}

// CMP16_SBC_FLAGS: carry-chain continuation for multi-word comparison.
// LD A,lhs_lo; SBC A,rhs_lo; LD A,lhs_hi; SBC A,rhs_hi
// Uses carry-in from previous CMP16_FLAGS or CMP16_SBC_FLAGS.
// Uses = [F] ensures regalloc won't insert flag-clobbering ops in between.
def CMP16_SBC_FLAGS : Z80Pseudo<(outs), (ins GR16:$lhs, GR16:$rhs)> {
  let Defs = [A, F];
  let Uses = [F];
  let mayLoad = false;
  let mayStore = false;
}

// CMP16_SBC_FLAGS against an immediate:
// LD A,lhs_lo; SBC A,lo; LD A,lhs_hi; SBC A,hi.
def CMP16_SBC_FLAGS_IMM : Z80Pseudo<(outs), (ins GR16:$lhs, i16imm:$rhs)> {
  let Defs = [A, F];
  let Uses = [F];
  let mayLoad = false;
  let mayStore = false;
}

// 16-bit shift right pseudo-instructions.
// Expanded in expandPostRAPseudo to SRL/SRA hi + RR lo.
def LSHR16 : Z80Pseudo<(outs GR16:$dst), (ins GR16:$src)> {
  let Constraints = "$dst = $src";
  let Defs = [F];
  let mayLoad = false;
  let mayStore = false;
}
def ASHR16 : Z80Pseudo<(outs GR16:$dst), (ins GR16:$src)> {
  let Constraints = "$dst = $src";
  let Defs = [F];
  let mayLoad = false;
  let mayStore = false;
}

// 16-bit sign extension: result = 0x0000 (positive) or 0xFFFF (negative)
// Expanded to: LD A,src_hi; ADD A,A; SBC A,A; LD dst_lo,A; LD dst_hi,A
// Used for G_ASHR by type_size-1 (e.g., ASHR i16, 15 for sext i16->i32)
def SEXT16 : Z80Pseudo<(outs GR16:$dst), (ins GR16:$src)> {
  let Defs = [A, F];
  let mayLoad = false;
  let mayStore = false;
}

// Variable shift pseudos - expanded to DJNZ loops in Z80ExpandPseudo.
// Pattern: INC B; DEC B; JR Z,done; .loop: <shift>; DJNZ .loop; .done:
// 8-bit: count in B, which is decremented to 0. The shifts are CB
// instructions that work in any register; the rotates are RLCA and RRCA,
// which work in A.
let mayLoad = false, mayStore = false in {
let Constraints = "$dst = $src", Uses = [B], Defs = [B, F] in {
def SHL8_VAR : Z80Pseudo<(outs GR8:$dst), (ins GR8:$src)>;
def LSHR8_VAR : Z80Pseudo<(outs GR8:$dst), (ins GR8:$src)>;
def ASHR8_VAR : Z80Pseudo<(outs GR8:$dst), (ins GR8:$src)>;
def ROTL8_VAR : Z80Pseudo<(outs Ac:$dst), (ins Ac:$src)>;
def ROTR8_VAR : Z80Pseudo<(outs Ac:$dst), (ins Ac:$src)>;
}
// 16-bit: value in HL, count in B, result in HL. B is decremented to 0.
def SHL16_VAR : Z80Pseudo<(outs), (ins)> {
  let Uses = [HL, B];
  let Defs = [HL, B, F];
}
def LSHR16_VAR : Z80Pseudo<(outs), (ins)> {
  let Uses = [HL, B];
  let Defs = [HL, B, F];
}
def ASHR16_VAR : Z80Pseudo<(outs), (ins)> {
  let Uses = [HL, B];
  let Defs = [HL, B, F];
}
// 8-bit multiply pseudo - expanded to DJNZ shift-add loop.
// Input: A = multiplier, E = multiplicand. Output: A = result (low 8 bits).
// Loop body: ADD A,A; RL D; JR NC,skip; ADD A,E; DJNZ loop
let Constraints = "$dst = $src", Uses = [E] in {
def MUL8 : Z80Pseudo<(outs Ac:$dst), (ins Ac:$src)> {
  let Defs = [B, D, F];
}
// 8-bit unsigned division pseudo - expanded to DJNZ restoring division loop.
// Input: A = dividend, E = divisor. Output: A = quotient.
// After loop: D = quotient, A = remainder; UDIV8 returns D, UMOD8 returns A.
def UDIV8 : Z80Pseudo<(outs Ac:$dst), (ins Ac:$src)> {
  let Defs = [B, D, F];
}
def UMOD8 : Z80Pseudo<(outs Ac:$dst), (ins Ac:$src)> {
  let Defs = [B, D, F];
}
// 8-bit signed division pseudo - sign-handles operands then restoring division.
// Input: A = dividend, E = divisor. Output: A = quotient (SDIV8) or remainder (SMOD8).
def SDIV8 : Z80Pseudo<(outs Ac:$dst), (ins Ac:$src)> {
  let Defs = [B, C, D, E, F];
}
def SMOD8 : Z80Pseudo<(outs Ac:$dst), (ins Ac:$src)> {
  let Defs = [B, C, D, E, F];
}
}
// 16-bit multiply pseudo (for +inline-i16-runtime mode).
// Input: HL = multiplicand, DE = multiplier. Output: DE = result.
// Expanded to 16-iteration shift-add loop in Z80ExpandPseudo.
def MUL16 : Z80Pseudo<(outs), (ins)> {
  let Uses = [HL, DE];
  let Defs = [A, B, C, DE, HL, F];
}
// 16-bit unsigned division pseudo (for +inline-i16-runtime mode).
// Input: HL = dividend, DE = divisor. Output: DE = quotient.
def UDIV16 : Z80Pseudo<(outs), (ins)> {
  let Uses = [HL, DE];
  let Defs = [A, B, C, DE, HL, F];
}
// 16-bit unsigned modulo pseudo. Output: DE = remainder.
def UMOD16 : Z80Pseudo<(outs), (ins)> {
  let Uses = [HL, DE];
  let Defs = [A, B, C, DE, HL, F];
}
// 16-bit signed division pseudo. Output: DE = quotient.
def SDIV16 : Z80Pseudo<(outs), (ins)> {
  let Uses = [HL, DE];
  let Defs = [A, B, C, DE, HL, F];
}
// 16-bit signed modulo pseudo. Output: DE = remainder.
def SMOD16 : Z80Pseudo<(outs), (ins)> {
  let Uses = [HL, DE];
  let Defs = [A, B, C, DE, HL, F];
}
// Block move for a length that is not a known non-zero constant.
// LD A,B; OR C; JR Z,.done; LDIR; .done:
// LDIR decrements BC before testing it, so BC == 0 would copy 65536 bytes.
def LDIR_GUARDED : Z80Pseudo<(outs), (ins)> {
  let Uses = [HL, DE, BC];
  let Defs = [HL, DE, BC, A, F];
  let mayLoad = true;
  let mayStore = true;
  // As on LDIR itself: passes that track memory invalidate on this.
  let hasSideEffects = 1;
}

let Constraints = "$dst = $lhs", Defs = [F] in {
// 8-bit unsigned saturating add pseudo.
// ADD A,src; JR NC,.done; LD A,#0xFF; .done:
def UADDSAT8 : Z80Pseudo<(outs Ac:$dst), (ins Ac:$lhs, GR8:$src)>;
// 8-bit unsigned saturating subtract pseudo.
// SUB src; JR NC,.done; XOR A; .done:
def USUBSAT8 : Z80Pseudo<(outs Ac:$dst), (ins Ac:$lhs, GR8:$src)>;
// 8-bit signed saturating add pseudo (Z80 only — uses P/V overflow flag).
// ADD A,src; JP PO,.done; RLCA; SBC A,A; XOR #0x80; .done:
def SADDSAT8 : Z80Pseudo<(outs Ac:$dst), (ins Ac:$lhs, GR8:$src)>;
// 8-bit signed saturating subtract pseudo (Z80 only — uses P/V overflow flag).
// SUB src; JP PO,.done; RLCA; SBC A,A; XOR #0x80; .done:
def SSUBSAT8 : Z80Pseudo<(outs Ac:$dst), (ins Ac:$lhs, GR8:$src)>;
}
} // mayLoad = false, mayStore = false

// Call frame setup/destroy pseudos for stack argument passing.
// These bracket call sequences where arguments are pushed onto the stack.
// ADJCALLSTACKDOWN is erased (PUSHes adjust SP), ADJCALLSTACKUP emits
// SP cleanup code in eliminateCallFramePseudoInstr.
let Defs = [SP], Uses = [SP], hasSideEffects = true,
    mayLoad = false, mayStore = false in {
def ADJCALLSTACKDOWN : Z80Pseudo<(outs), (ins i16imm:$bytes, i16imm:$prior)>;
}
// ADJCALLSTACKUP's expansion may clobber A (POP AF for small amounts) or HL
// (LD HL,N; ADD HL,SP; LD SP,HL for large amounts), but only when the caller
// itself has bytes to pop. Call lowering knows the amount and adds the
// matching implicit-def operands there; declaring the scratch registers
// statically killed a call's return-value def on every callee-cleanup call,
// where the pseudo erases to nothing.
let Defs = [SP], Uses = [SP], hasSideEffects = true,
    mayLoad = false, mayStore = false in {
def ADJCALLSTACKUP : Z80Pseudo<(outs), (ins i16imm:$bytes, i16imm:$prior)>;
}

// Callee-cleanup return: pops return address, adjusts SP by $bytes, then
// returns via indirect jump. Used by __sdcccall(1) callee-cleanup and
// future __z88dk_callee convention.
let isReturn = true, isTerminator = true, isBarrier = true,
    Defs = [SP, HL, BC], Uses = [SP], hasSideEffects = true,
    mayLoad = false, mayStore = false in {
def RET_CLEANUP : Z80Pseudo<(outs), (ins i16imm:$bytes)>;
}

// Tail calls
let isCall = true, isReturn = true, isTerminator = true, isBarrier = true,
    hasSideEffects = true, mayLoad = true, mayStore = true in {
let Uses = [SP], Defs = [A, BC, DE, HL, IY, F] in
def TCRETURN_nn : Z80Pseudo<(outs), (ins target16:$dst)>,
                  PseudoInstExpansion<(JP_nn target16:$dst)>;
let Uses = [SP, IY], Defs = [A, BC, DE, HL, IY, F] in
def TCRETURN_IY : Z80Pseudo<(outs), (ins)>, PseudoInstExpansion<(JP_IYind)>;
let Uses = [SP, HL], Defs = [A, BC, DE, HL, F] in
def TCRETURN_HL : Z80Pseudo<(outs), (ins)>, PseudoInstExpansion<(JP_HLind)>;
}

// Conditional tail calls. The path that falls through sees no register
// change.
let isCall = true, isReturn = true, isTerminator = true,
    hasSideEffects = true, mayLoad = true, mayStore = true,
    Uses = [SP, F] in {
def TCRETURN_NZ_nn : Z80Pseudo<(outs), (ins target16:$dst)>,
                     PseudoInstExpansion<(JP_NZ_nn target16:$dst)>;
def TCRETURN_Z_nn : Z80Pseudo<(outs), (ins target16:$dst)>,
                    PseudoInstExpansion<(JP_Z_nn target16:$dst)>;
def TCRETURN_NC_nn : Z80Pseudo<(outs), (ins target16:$dst)>,
                     PseudoInstExpansion<(JP_NC_nn target16:$dst)>;
def TCRETURN_C_nn : Z80Pseudo<(outs), (ins target16:$dst)>,
                    PseudoInstExpansion<(JP_C_nn target16:$dst)>;
}

// SM83-only instructions and pseudos
include "SM83InstrInfo.td"

// GlobalISel pseudo-instructions for code generation
include "Z80InstrGISel.td"
