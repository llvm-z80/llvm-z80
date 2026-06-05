//===-- Z80InstrFormats.td - Z80 Instruction Formats --------*- tablegen -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Z80 Instruction Format Definitions.
//
// Z80 instruction encoding uses optional prefix bytes:
//   - Unprefixed: 1-3 bytes (opcode + optional operands)
//   - CB prefix:  Bit manipulation instructions
//   - DD prefix:  IX-indexed instructions
//   - FD prefix:  IY-indexed instructions
//   - ED prefix:  Extended instructions
//   - DD CB/FD CB: Indexed bit manipulation (4 bytes)
//
//===----------------------------------------------------------------------===//

/// Predicates for Z80 variants
def HasZ80 : Predicate<"Subtarget->hasZ80()">,
             AssemblerPredicate<(all_of FeatureZ80), "Z80"> {
  let PredicateName = "FeatureZ80";
}

def HasZ180 : Predicate<"Subtarget->hasZ180()">,
              AssemblerPredicate<(all_of FeatureZ180), "Z180"> {
  let PredicateName = "FeatureZ180";
}

def HasR800 : Predicate<"Subtarget->hasR800()">,
              AssemblerPredicate<(all_of FeatureR800), "R800"> {
  let PredicateName = "FeatureR800";
}

def HasEZ80 : Predicate<"Subtarget->hasEZ80()">,
              AssemblerPredicate<(all_of FeatureEZ80), "eZ80"> {
  let PredicateName = "FeatureEZ80";
}

def HasSM83 : Predicate<"Subtarget->hasSM83()">,
              AssemblerPredicate<(all_of FeatureSM83), "SM83"> {
  let PredicateName = "FeatureSM83";
}

//===----------------------------------------------------------------------===//
//  Operand definitions
//===----------------------------------------------------------------------===//

/// Operand matching classes
class Z80AsmOperand<string name> : AsmOperandClass {
  let Name = name;
  let DiagnosticType = !strconcat("Invalid", name);
}

class ImmediateAsmOperand<string name> : Z80AsmOperand<name> {
  let RenderMethod = "addImmOperands";
  let DiagnosticType = "immediate";
}

class PCRelativeOperand<string name> : Z80AsmOperand<name>;
class Addr16Operand<string name> : Z80AsmOperand<name>;

/// 3-bit immediate operand (0-7) for bit operations and RST
def imm3 : Operand<i32> {
  let ParserMatchClass = ImmediateAsmOperand<"Imm3">;
  let DecoderMethod = "decodeUImmOperand<3>";
  let OperandType = "OPERAND_IMM3";
  let OperandNamespace = "Z80Op";
  let Type = i8;
}

/// 8-bit immediate operand
class imm8at<int offset> : Operand<i32> {
  let ParserMatchClass = ImmediateAsmOperand<"Imm8">;
  let EncoderMethod = "encodeImm<Z80::Imm8, " # offset # ">";
  let DecoderMethod = "decodeUImmOperand<8>";
  let OperandType = "OPERAND_IMM8";
  let OperandNamespace = "Z80Op";
  let Type = i8;
}
def imm8 : imm8at<1>;
def imm8at2 : imm8at<2>;
def imm8at3 : imm8at<3>;

/// 16-bit immediate operand
class imm16at<int offset> : Operand<i32> {
  let ParserMatchClass = ImmediateAsmOperand<"Imm16">;
  let EncoderMethod = "encodeImm<Z80::Imm16, " # offset # ">";
  let DecoderMethod = "decodeUImmOperand<16>";
  let OperandType = "OPERAND_IMM16";
  let OperandNamespace = "Z80Op";
  let Type = i16;
}
def imm16 : imm16at<1>;
def imm16at2 : imm16at<2>;

/// 8-bit signed PC-relative offset for JR/DJNZ
class pcrel8at<int offset> : Operand<i32> {
  let ParserMatchClass = PCRelativeOperand<"PCRel8">;
  let EncoderMethod = "encodeImm<Z80::PCRel8, " # offset # ">";
  let DecoderMethod = "decodeSImmOperand<8>";
  let PrintMethod = "printBranchOperand";
  let OperandType = "OPERAND_PCREL";
}
def pcrel8 : pcrel8at<1>;

/// 16-bit address
class addr16at<int offset> : Operand<i32> {
  let ParserMatchClass = Addr16Operand<"Addr16">;
  let EncoderMethod = "encodeImm<Z80::Addr16, " # offset # ">";
  let DecoderMethod = "decodeUImmOperand<16>";
  let OperandType = "OPERAND_ADDR16";
  let OperandNamespace = "Z80Op";
  let Type = i16;
}
def addr16 : addr16at<1>;
def addr16at2 : addr16at<2>;

/// 8-bit signed immediate operand (for SM83 ADD SP,e / LDHL SP,e)
class simm8at<int offset> : Operand<i32> {
  let ParserMatchClass = ImmediateAsmOperand<"Imm8">;
  let EncoderMethod = "encodeImm<Z80::Imm8, " # offset # ">";
  let DecoderMethod = "decodeSImmOperand<8>";
  let OperandType = "OPERAND_IMM8";
  let OperandNamespace = "Z80Op";
  let Type = i8;
}
def simm8 : simm8at<1>;

/// 8-bit signed displacement for indexed addressing (IX+d, IY+d)
class disp8at<int offset> : Operand<i32> {
  let ParserMatchClass = ImmediateAsmOperand<"Disp8">;
  let EncoderMethod = "encodeImm<Z80::Disp8, " # offset # ">";
  let DecoderMethod = "decodeSImmOperand<8>";
  let OperandType = "OPERAND_DISP8";
  let OperandNamespace = "Z80Op";
  let Type = i8;
  let PrintMethod = "printIndexedOperand";
}
def disp8 : disp8at<2>;    // After DD/FD + opcode
def disp8at3 : disp8at<3>; // After DD/FD CB + displacement

/// Port number for IN/OUT
def port8 : Operand<i32> {
  let ParserMatchClass = ImmediateAsmOperand<"Port8">;
  let DecoderMethod = "decodeUImmOperand<8>";
  let OperandType = "OPERAND_PORT8";
  let OperandNamespace = "Z80Op";
  let Type = i8;
}

//===----------------------------------------------------------------------===//
//  Base instruction class
//===----------------------------------------------------------------------===//

class Z80Inst<string asmstr> : Instruction {
  let Namespace = "Z80";

  let OutOperandList = (outs);
  let AsmString = asmstr;

  // Conservative defaults for MC layer
  let mayLoad = true;
  let mayStore = true;
  let hasSideEffects = true;

  let DecoderNamespace = "Z80";
}

class InstConditionalBranch {
  bit isBranch = true;
  bit isTerminator = true;
}

class InstUnconditionalBranch : InstConditionalBranch {
  bit isBarrier = true;
}

class InstCall {
  bit isCall = true;
}

class InstReturn {
  bit isBarrier = true;
  bit isReturn = true;
  bit isTerminator = true;
}

//===----------------------------------------------------------------------===//
//  Addressing modes
//===----------------------------------------------------------------------===//

class AddressingMode {
  string OperandsStr = ?;
  dag InOperandList = ?;
}

def Implied : AddressingMode {
  let OperandsStr = "";
  let InOperandList = (ins);
}

// Note: All 8-bit operands use 'imm' as the field name to match encoding
def Imm8Mode : AddressingMode {
  let OperandsStr = "$imm";
  let InOperandList = (ins imm8:$imm);
}

def Imm16Mode : AddressingMode {
  let OperandsStr = "$imm";
  let InOperandList = (ins imm16:$imm);
}

// DirectMode uses 'imm' for encoding compatibility (printed as address)
def DirectMode : AddressingMode {
  let OperandsStr = "( $imm )";
  let InOperandList = (ins addr16:$imm);
}

// RelativeMode uses 'imm' for encoding compatibility
def RelativeMode : AddressingMode {
  let OperandsStr = "$imm";
  let InOperandList = (ins pcrel8:$imm);
}

// IndexedIXMode uses 'imm' for encoding compatibility
def IndexedIXMode : AddressingMode {
  let OperandsStr = "( ix + $imm )";
  let InOperandList = (ins disp8:$imm);
}

// IndexedIYMode uses 'imm' for encoding compatibility
def IndexedIYMode : AddressingMode {
  let OperandsStr = "( iy + $imm )";
  let InOperandList = (ins disp8:$imm);
}

// PortMode uses 'imm' for encoding compatibility
def PortMode : AddressingMode {
  let OperandsStr = "( $imm )";
  let InOperandList = (ins port8:$imm);
}

//===----------------------------------------------------------------------===//
//  Unprefixed instruction formats
//===----------------------------------------------------------------------===//

/// 1-byte unprefixed instruction (NOP, HALT, RET, etc.)
class Inst8<string asmstr, bits<8> opcode> : Z80Inst<asmstr> {
  let Size = 1;
  bits<8> Inst;
  let Inst = opcode;
  let InOperandList = (ins);
}

/// 2-byte unprefixed instruction with 8-bit immediate
class Inst16Imm<string asmstr, bits<8> opcode> : Z80Inst<asmstr> {
  let Size = 2;
  bits<8> imm;
  bits<16> Inst;
  let Inst{7-0} = opcode;
  let Inst{15-8} = imm;
  let InOperandList = (ins imm8:$imm);
  let AsmString = asmstr # " $imm";
}

/// 2-byte unprefixed instruction with signed 8-bit immediate
/// Used for SM83 LDHL SP,e and ADD SP,e which take signed offsets.
class Inst16SImm<string asmstr, bits<8> opcode> : Z80Inst<asmstr> {
  let Size = 2;
  bits<8> imm;
  bits<16> Inst;
  let Inst{7-0} = opcode;
  let Inst{15-8} = imm;
  let InOperandList = (ins simm8:$imm);
  let AsmString = asmstr # " $imm";
}

/// 2-byte unprefixed instruction with 8-bit port
class Inst16Port<string asmstr, bits<8> opcode> : Z80Inst<asmstr> {
  let Size = 2;
  bits<8> port;
  bits<16> Inst;
  let Inst{7-0} = opcode;
  let Inst{15-8} = port;
  let InOperandList = (ins port8:$port);
  let AsmString = asmstr # " ($port)";
}

/// 2-byte unprefixed instruction with PC-relative target
class Inst16Rel<string asmstr, bits<8> opcode> : Z80Inst<asmstr> {
  let Size = 2;
  bits<8> target;
  bits<16> Inst;
  let Inst{7-0} = opcode;
  let Inst{15-8} = target;
  let InOperandList = (ins pcrel8:$target);
  let AsmString = asmstr # " $target";
}

/// Legacy 2-byte unprefixed instruction (for backward compat, no operand encoding)
class Inst16<string asmstr, bits<8> opcode, AddressingMode mode = Imm8Mode>
    : Z80Inst<asmstr> {
  let Size = 2;
  bits<8> imm;
  bits<16> Inst;
  let Inst{7-0} = opcode;
  let Inst{15-8} = imm;
  let InOperandList = mode.InOperandList;
  let AsmString = asmstr # " " # mode.OperandsStr;
}

/// 3-byte unprefixed instruction with 16-bit immediate
class Inst24Imm<string asmstr, bits<8> opcode> : Z80Inst<asmstr> {
  let Size = 3;
  bits<16> imm;
  bits<24> Inst;
  let Inst{7-0} = opcode;
  let Inst{23-8} = imm;
  let InOperandList = (ins imm16:$imm);
  let AsmString = asmstr # " $imm";
}

/// 3-byte unprefixed instruction with 16-bit address
class Inst24Addr<string asmstr, bits<8> opcode> : Z80Inst<asmstr> {
  let Size = 3;
  bits<16> addr;
  bits<24> Inst;
  let Inst{7-0} = opcode;
  let Inst{23-8} = addr;
  let InOperandList = (ins addr16:$addr);
  let AsmString = asmstr # " ($addr)";
}

/// Legacy 3-byte unprefixed instruction (for backward compat)
class Inst24<string asmstr, bits<8> opcode, AddressingMode mode = Imm16Mode>
    : Z80Inst<asmstr> {
  let Size = 3;
  bits<16> imm;
  bits<24> Inst;
  let Inst{7-0} = opcode;
  let Inst{23-8} = imm;
  let InOperandList = mode.InOperandList;
  let AsmString = asmstr # " " # mode.OperandsStr;
}

//===----------------------------------------------------------------------===//
//  CB-prefixed instructions (bit manipulation)
//===----------------------------------------------------------------------===//

/// 2-byte CB-prefixed instruction (RLC r, BIT b,r, etc.)
class InstCB<string asmstr, bits<8> opcode> : Z80Inst<asmstr> {
  let Size = 2;
  bits<16> Inst;
  let Inst{7-0} = 0xCB;
  let Inst{15-8} = opcode;
  let InOperandList = (ins);
}

//===----------------------------------------------------------------------===//
//  DD-prefixed instructions (IX operations)
//===----------------------------------------------------------------------===//

/// 2-byte DD-prefixed instruction
class InstDD8<string asmstr, bits<8> opcode> : Z80Inst<asmstr> {
  let Size = 2;
  bits<16> Inst;
  let Inst{7-0} = 0xDD;
  let Inst{15-8} = opcode;
  let InOperandList = (ins);
}

/// 3-byte DD-prefixed instruction with 8-bit operand (IX+d addressing)
class InstDD16<string asmstr, bits<8> opcode, AddressingMode mode = IndexedIXMode>
    : Z80Inst<asmstr> {
  let Size = 3;
  bits<8> imm;
  bits<24> Inst;
  let Inst{7-0} = 0xDD;
  let Inst{15-8} = opcode;
  let Inst{23-16} = imm;
  let InOperandList = mode.InOperandList;
  let AsmString = asmstr # " " # mode.OperandsStr;
}

/// 4-byte DD-prefixed instruction with displacement + 8-bit immediate
/// Encoding: DD opcode disp imm (e.g., LD (IX+d),n = DD 36 d n)
class InstDD16Imm<string asmstr, bits<8> opcode> : Z80Inst<asmstr> {
  let Size = 4;
  bits<8> disp;
  bits<8> val;
  bits<32> Inst;
  let Inst{7-0} = 0xDD;
  let Inst{15-8} = opcode;
  let Inst{23-16} = disp;
  let Inst{31-24} = val;
  let InOperandList = (ins disp8:$disp, imm8at3:$val);
}

/// 4-byte DD-prefixed instruction with 16-bit operand
class InstDD24<string asmstr, bits<8> opcode, AddressingMode mode = Imm16Mode>
    : Z80Inst<asmstr> {
  let Size = 4;
  bits<16> imm;
  bits<32> Inst;
  let Inst{7-0} = 0xDD;
  let Inst{15-8} = opcode;
  let Inst{31-16} = imm;
  let InOperandList = mode.InOperandList;
  let AsmString = asmstr # " " # mode.OperandsStr;
}

//===----------------------------------------------------------------------===//
//  FD-prefixed instructions (IY operations)
//===----------------------------------------------------------------------===//

/// 2-byte FD-prefixed instruction
class InstFD8<string asmstr, bits<8> opcode> : Z80Inst<asmstr> {
  let Size = 2;
  bits<16> Inst;
  let Inst{7-0} = 0xFD;
  let Inst{15-8} = opcode;
  let InOperandList = (ins);
}

/// 3-byte FD-prefixed instruction with 8-bit operand (IY+d addressing)
class InstFD16<string asmstr, bits<8> opcode, AddressingMode mode = IndexedIYMode>
    : Z80Inst<asmstr> {
  let Size = 3;
  bits<8> imm;
  bits<24> Inst;
  let Inst{7-0} = 0xFD;
  let Inst{15-8} = opcode;
  let Inst{23-16} = imm;
  let InOperandList = mode.InOperandList;
  let AsmString = asmstr # " " # mode.OperandsStr;
}

/// 4-byte FD-prefixed instruction with displacement + 8-bit immediate
/// Encoding: FD opcode disp imm (e.g., LD (IY+d),n = FD 36 d n)
class InstFD16Imm<string asmstr, bits<8> opcode> : Z80Inst<asmstr> {
  let Size = 4;
  bits<8> disp;
  bits<8> val;
  bits<32> Inst;
  let Inst{7-0} = 0xFD;
  let Inst{15-8} = opcode;
  let Inst{23-16} = disp;
  let Inst{31-24} = val;
  let InOperandList = (ins disp8:$disp, imm8at3:$val);
}

/// 4-byte FD-prefixed instruction with 16-bit operand
class InstFD24<string asmstr, bits<8> opcode, AddressingMode mode = Imm16Mode>
    : Z80Inst<asmstr> {
  let Size = 4;
  bits<16> imm;
  bits<32> Inst;
  let Inst{7-0} = 0xFD;
  let Inst{15-8} = opcode;
  let Inst{31-16} = imm;
  let InOperandList = mode.InOperandList;
  let AsmString = asmstr # " " # mode.OperandsStr;
}

//===----------------------------------------------------------------------===//
//  ED-prefixed instructions (extended)
//===----------------------------------------------------------------------===//

/// 2-byte ED-prefixed instruction
class InstED8<string asmstr, bits<8> opcode> : Z80Inst<asmstr> {
  let Size = 2;
  bits<16> Inst;
  let Inst{7-0} = 0xED;
  let Inst{15-8} = opcode;
  let InOperandList = (ins);
}

/// 4-byte ED-prefixed instruction with 16-bit operand
class InstED24<string asmstr, bits<8> opcode, AddressingMode mode = DirectMode>
    : Z80Inst<asmstr> {
  let Size = 4;
  bits<16> imm;
  bits<32> Inst;
  let Inst{7-0} = 0xED;
  let Inst{15-8} = opcode;
  let Inst{31-16} = imm;
  let InOperandList = mode.InOperandList;
  let AsmString = asmstr # " " # mode.OperandsStr;
}

//===----------------------------------------------------------------------===//
//  DD CB / FD CB prefixed instructions (indexed bit manipulation)
//===----------------------------------------------------------------------===//

/// 4-byte DD CB indexed bit instruction
class InstDDCB<string asmstr, bits<8> opcode, AddressingMode mode = IndexedIXMode>
    : Z80Inst<asmstr> {
  let Size = 4;
  bits<8> imm;
  bits<32> Inst;
  let Inst{7-0} = 0xDD;
  let Inst{15-8} = 0xCB;
  let Inst{23-16} = imm;
  let Inst{31-24} = opcode;
  let InOperandList = mode.InOperandList;
  let AsmString = asmstr # " " # mode.OperandsStr;
}

/// 4-byte FD CB indexed bit instruction
class InstFDCB<string asmstr, bits<8> opcode, AddressingMode mode = IndexedIYMode>
    : Z80Inst<asmstr> {
  let Size = 4;
  bits<8> imm;
  bits<32> Inst;
  let Inst{7-0} = 0xFD;
  let Inst{15-8} = 0xCB;
  let Inst{23-16} = imm;
  let Inst{31-24} = opcode;
  let InOperandList = mode.InOperandList;
  let AsmString = asmstr # " " # mode.OperandsStr;
}

//===----------------------------------------------------------------------===//
//  Condition code encoding
//===----------------------------------------------------------------------===//

// Z80 condition codes: NZ=0, Z=1, NC=2, C=3, PO=4, PE=5, P=6, M=7
class Z80CondCode<bits<3> val, string name> {
  bits<3> cc = val;
  string Name = name;
}

def CC_NZ : Z80CondCode<0b000, "nz">;
def CC_Z  : Z80CondCode<0b001, "z">;
def CC_NC : Z80CondCode<0b010, "nc">;
def CC_C  : Z80CondCode<0b011, "c">;
def CC_PO : Z80CondCode<0b100, "po">;
def CC_PE : Z80CondCode<0b101, "pe">;
def CC_P  : Z80CondCode<0b110, "p">;
def CC_M  : Z80CondCode<0b111, "m">;

//===----------------------------------------------------------------------===//
//  Register encoding
//===----------------------------------------------------------------------===//

// Z80 8-bit register encoding: B=0, C=1, D=2, E=3, H=4, L=5, (HL)=6, A=7
class RegEnc8<bits<3> val, string name> {
  bits<3> enc = val;
  string Name = name;
}

def REG_B   : RegEnc8<0b000, "b">;
def REG_C   : RegEnc8<0b001, "c">;
def REG_D   : RegEnc8<0b010, "d">;
def REG_E   : RegEnc8<0b011, "e">;
def REG_H   : RegEnc8<0b100, "h">;
def REG_L   : RegEnc8<0b101, "l">;
def REG_HL_IND : RegEnc8<0b110, "(hl)">;
def REG_A   : RegEnc8<0b111, "a">;

// Z80 16-bit register pair encoding: BC=0, DE=1, HL=2, SP=3 (or AF for PUSH/POP)
class RegPairEnc<bits<2> val, string name> {
  bits<2> enc = val;
  string Name = name;
}

def REGPAIR_BC : RegPairEnc<0b00, "bc">;
def REGPAIR_DE : RegPairEnc<0b01, "de">;
def REGPAIR_HL : RegPairEnc<0b10, "hl">;
def REGPAIR_SP : RegPairEnc<0b11, "sp">;
def REGPAIR_AF : RegPairEnc<0b11, "af">;

//===----------------------------------------------------------------------===//
//  Instruction pattern helpers
//===----------------------------------------------------------------------===//

/// LD r,n - Load 8-bit immediate into register
/// Opcode: 0b00rrr110 where rrr is register encoding
class LDrn<RegEnc8 r> : Inst16<"ld " # r.Name # ",",
                               !or(0x06, !shl(r.enc, 3)), Imm8Mode>;

/// LD rr,nn - Load 16-bit immediate into register pair
/// Opcode: 0b00rr0001 where rr is register pair encoding
class LDrrnn<RegPairEnc rr> : Inst24<"ld " # rr.Name # ",",
                                     !or(0x01, !shl(rr.enc, 4)), Imm16Mode>;

/// JR cc,e - Conditional relative jump
/// Opcode: 0b001cc000 where cc is condition (only NZ/Z/NC/C for JR)
class JRcond<Z80CondCode cc> : Inst16<"jr " # cc.Name # ",",
                                      !or(0x20, !shl(cc.cc, 3)), RelativeMode>,
                               InstConditionalBranch {
  let Uses = [FLAGS];
}

/// JP cc,nn - Conditional absolute jump
/// Opcode: 0b11ccc010 where ccc is condition
class JPcond<Z80CondCode cc> : Inst24<"jp " # cc.Name # ",",
                                      !or(0xC2, !shl(cc.cc, 3)), Imm16Mode>,
                               InstConditionalBranch {
  let Uses = [FLAGS];
}

/// CALL cc,nn - Conditional call
/// Opcode: 0b11ccc100 where ccc is condition
/// IY: caller-saved on Z80; harmless on SM83 where IY is reserved.
class CALLcond<Z80CondCode cc> : Inst24<"call " # cc.Name # ",",
                                        !or(0xC4, !shl(cc.cc, 3)), Imm16Mode>,
                                 InstCall {
  let Uses = [FLAGS];
  let Defs = [A, BC, DE, HL, IY, FLAGS];
}

/// RET cc - Conditional return
/// Opcode: 0b11ccc000 where ccc is condition
class RETcond<Z80CondCode cc> : Inst8<"ret " # cc.Name,
                                      !or(0xC0, !shl(cc.cc, 3))>,
                                InstConditionalBranch {
  let Uses = [FLAGS];
}

/// RST n - Restart to vector
/// Opcode: 0b11ttt111 where ttt*8 is the restart address
class RSTvec<bits<3> t> : Inst8<"rst " # !mul(t, 8),
                                !or(0xC7, !shl(t, 3))>,
                          InstCall;

/// PUSH rr - Push register pair
/// Opcode: 0b11rr0101 where rr is register pair (BC/DE/HL/AF)
class PUSHrr<RegPairEnc rr> : Inst8<"push " # rr.Name,
                                    !or(0xC5, !shl(rr.enc, 4))>;

/// POP rr - Pop register pair
/// Opcode: 0b11rr0001 where rr is register pair (BC/DE/HL/AF)
class POPrr<RegPairEnc rr> : Inst8<"pop " # rr.Name,
                                   !or(0xC1, !shl(rr.enc, 4))>;

/// INC rr - Increment register pair
/// Opcode: 0b00rr0011 where rr is register pair
class INCrr<RegPairEnc rr> : Inst8<"inc " # rr.Name,
                                   !or(0x03, !shl(rr.enc, 4))>;

/// DEC rr - Decrement register pair
/// Opcode: 0b00rr1011 where rr is register pair
class DECrr<RegPairEnc rr> : Inst8<"dec " # rr.Name,
                                   !or(0x0B, !shl(rr.enc, 4))>;

/// INC r - Increment 8-bit register
/// Opcode: 0b00rrr100 where rrr is register encoding
class INCr<RegEnc8 r> : Inst8<"inc " # r.Name,
                              !or(0x04, !shl(r.enc, 3))>;

/// DEC r - Decrement 8-bit register
/// Opcode: 0b00rrr101 where rrr is register encoding
class DECr<RegEnc8 r> : Inst8<"dec " # r.Name,
                              !or(0x05, !shl(r.enc, 3))>;

/// ADD HL,rr - Add register pair to HL
/// Opcode: 0b00rr1001 where rr is register pair
class ADDHLrr<RegPairEnc rr> : Inst8<"add hl," # rr.Name,
                                     !or(0x09, !shl(rr.enc, 4))>;

//===----------------------------------------------------------------------===//
//  CB-prefix bit operation helpers
//===----------------------------------------------------------------------===//

/// CB-prefixed rotation/shift on register
/// Opcode pattern: CB 0b00ooorrr where ooo is operation, rrr is register
/// Note: Defs/Uses are added per-instruction in Z80InstrInfo.td since
/// RegEnc8 doesn't carry a Register reference.
class CBRotate<bits<3> op, RegEnc8 r, string opname>
    : InstCB<opname # " " # r.Name, !or(!shl(op, 3), r.enc)>;

/// BIT b,r - Test bit b of register r
/// Opcode: CB 0b01bbbrrr where bbb is bit number, rrr is register
class BITbr<bits<3> b, RegEnc8 r>
    : InstCB<"bit " # b # "," # r.Name, !or(0x40, !shl(b, 3), r.enc)>;

/// RES b,r - Reset bit b of register r
/// Opcode: CB 0b10bbbrrr where bbb is bit number, rrr is register
class RESbr<bits<3> b, RegEnc8 r>
    : InstCB<"res " # b # "," # r.Name, !or(0x80, !shl(b, 3), r.enc)>;

/// SET b,r - Set bit b of register r
/// Opcode: CB 0b11bbbrrr where bbb is bit number, rrr is register
class SETbr<bits<3> b, RegEnc8 r>
    : InstCB<"set " # b # "," # r.Name, !or(0xC0, !shl(b, 3), r.enc)>;

//===----------------------------------------------------------------------===//
//  Operand type for labels
//===----------------------------------------------------------------------===//

def label : Operand<OtherVT>;
