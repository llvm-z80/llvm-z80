//===----------------------------------------------------------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the predicates, operands and encoding classes that the Z80
// and SM83 instruction definitions use.
//
//===----------------------------------------------------------------------===//

def IsZ80 : Predicate<"Subtarget->isZ80()">,
            AssemblerPredicate<(all_of FeatureZ80), "Z80">;

def IsSM83 : Predicate<"Subtarget->isSM83()">,
             AssemblerPredicate<(all_of FeatureSM83), "SM83">;

//===----------------------------------------------------------------------===//
//  Operand definitions
//===----------------------------------------------------------------------===//

// Immediate, address and branch target operands
class ImmAsmOperand<string name> : AsmOperandClass {
  let Name = name;
  let RenderMethod = "addImmOperands";
  let DiagnosticType = "immediate";
}

// JR/DJNZ target. A constant is an address.
def PCRel8AsmOperand : AsmOperandClass {
  let Name = "PCRel8";
  let PredicateMethod = "isImm";
  let RenderMethod = "addPCRelOperands";
}

class Z80Op<ValueType vt> : Operand<vt> {
  let OperandNamespace = "Z80Op";
}

// 3-bit bit number for BIT, RES and SET
def imm3 : Operand<i8> {
  let ParserMatchClass = ImmAsmOperand<"Imm3">;
  let DecoderMethod = "decodeUImmOperand<3>";
  let OperandType = "OPERAND_IMMEDIATE";
}

// 8-bit immediate for LD r,n and ALU instructions
class imm8at<int offset> : Z80Op<i8> {
  let ParserMatchClass = ImmAsmOperand<"Imm8">;
  let EncoderMethod = "encodeImm<Z80::Imm8, " # offset # ">";
  let DecoderMethod = "decodeUImmOperand<8>";
  let OperandType = "OPERAND_IMM_DATA";
}
def imm8 : imm8at<1>;
def imm8at3 : imm8at<3>;

// 16-bit immediate for LD rr,nn
class imm16at<int offset> : Z80Op<i16> {
  let ParserMatchClass = ImmAsmOperand<"Imm16">;
  let EncoderMethod = "encodeImm<Z80::Imm16, " # offset # ">";
  let DecoderMethod = "decodeUImmOperand<16>";
  let OperandType = "OPERAND_IMM_DATA";
}
def imm16 : imm16at<1>;
def imm16at2 : imm16at<2>;

// 16-bit memory address for LD (nn)
class addr16at<int offset> : Operand<i16> {
  let ParserMatchClass = ImmAsmOperand<"Addr16">;
  let EncoderMethod = "encodeImm<Z80::Addr16, " # offset # ">";
  let DecoderMethod = "decodeUImmOperand<16>";
  let OperandType = "OPERAND_MEMORY";
}
def addr16 : addr16at<1>;
def addr16at2 : addr16at<2>;

// 16-bit absolute target for JP nn and CALL nn
def target16 : Operand<i16> {
  let ParserMatchClass = ImmAsmOperand<"Addr16">;
  let EncoderMethod = "encodeImm<Z80::Addr16, 1>";
  let DecoderMethod = "decodeUImmOperand<16>";
  let OperandType = "OPERAND_IMMEDIATE";
}

// 8-bit signed PC-relative offset for JR/DJNZ
def pcrel8 : Operand<OtherVT> {
  let ParserMatchClass = PCRel8AsmOperand;
  let EncoderMethod = "encodeImm<Z80::PCRel8, 1>";
  let DecoderMethod = "decodeSImmOperand<8>";
  let PrintMethod = "printBranchOperand";
  let OperandType = "OPERAND_PCREL";
}

// 8-bit signed offset for SM83 ADD SP,e and LDHL SP,e
def simm8 : Z80Op<i8> {
  let ParserMatchClass = ImmAsmOperand<"SImm8">;
  let EncoderMethod = "encodeImm<Z80::Disp8, 1>";
  let DecoderMethod = "decodeSImmOperand<8>";
  let OperandType = "OPERAND_IMM_DATA";
}

// 8-bit signed displacement for (IX+d) and (IY+d)
def disp8 : Operand<i8> {
  let ParserMatchClass = ImmAsmOperand<"SImm8">;
  let EncoderMethod = "encodeImm<Z80::Disp8, 2>";
  let DecoderMethod = "decodeSImmOperand<8>";
  let PrintMethod = "printIndexedOperand";
  let OperandType = "OPERAND_IMMEDIATE";
}

// 8-bit port number for IN A,(n) and OUT (n),A
def port8 : Operand<i8> {
  let ParserMatchClass = ImmAsmOperand<"Port8">;
  let EncoderMethod = "encodeImm<Z80::Addr8, 1>";
  let DecoderMethod = "decodeUImmOperand<8>";
  let OperandType = "OPERAND_IMMEDIATE";
}

// 8-bit offset from 0xFF00 for SM83 LDH A,(n) and LDH (n),A
def hram8 : Operand<i8> {
  let ParserMatchClass = ImmAsmOperand<"HRAM8">;
  let EncoderMethod = "encodeImm<Z80::LDH8, 1>";
  let DecoderMethod = "decodeUImmOperand<8>";
  let OperandType = "OPERAND_IMMEDIATE";
}

//===----------------------------------------------------------------------===//
//  Base instruction class
//===----------------------------------------------------------------------===//

// Base class for Z80 instructions
class Z80Inst<string asmstr> : Instruction {
  let Namespace = "Z80";
  let DecoderNamespace = "Z80";
  let OutOperandList = (outs);
  let AsmString = asmstr;

  // guessInstructionProperties is off, so we need default values for these.
  let mayLoad = true;
  let mayStore = true;
  let hasSideEffects = true;
}

// Base classes by instruction size
class Z80Inst8<string asmstr> : Z80Inst<asmstr> {
  field bits<8> Inst;
  let Size = 1;
}

class Z80Inst16<string asmstr> : Z80Inst<asmstr> {
  field bits<16> Inst;
  let Size = 2;
}

class Z80Inst24<string asmstr> : Z80Inst<asmstr> {
  field bits<24> Inst;
  let Size = 3;
}

class Z80Inst32<string asmstr> : Z80Inst<asmstr> {
  field bits<32> Inst;
  let Size = 4;
}

// Base class for pseudo instructions
class Z80Pseudo<dag outs, dag ins> : Instruction {
  let Namespace = "Z80";
  let OutOperandList = outs;
  let InOperandList = ins;
  let hasSideEffects = false;
  let isPseudo = true;
  let isCodeGenOnly = true;
  let hasNoSchedulingInfo = true;
}

//===----------------------------------------------------------------------===//
//  Addressing modes
//===----------------------------------------------------------------------===//

// Note: All operands use 'imm' as the field name to match encoding
class AddressingMode {
  string OperandsStr = ?;
  dag InOperandList = ?;
}

def Imm8Mode : AddressingMode {
  let OperandsStr = "$imm";
  let InOperandList = (ins imm8:$imm);
}

def Imm16Mode : AddressingMode {
  let OperandsStr = "$imm";
  let InOperandList = (ins imm16:$imm);
}

def TargetMode : AddressingMode {
  let OperandsStr = "$imm";
  let InOperandList = (ins target16:$imm);
}

def DirectMode : AddressingMode {
  let OperandsStr = "( $imm )";
  let InOperandList = (ins addr16:$imm);
}

def DirectAt2Mode : AddressingMode {
  let OperandsStr = "( $imm )";
  let InOperandList = (ins addr16at2:$imm);
}

def Imm16At2Mode : AddressingMode {
  let OperandsStr = "$imm";
  let InOperandList = (ins imm16at2:$imm);
}

def RelativeMode : AddressingMode {
  let OperandsStr = "$imm";
  let InOperandList = (ins pcrel8:$imm);
}

def PortMode : AddressingMode {
  let OperandsStr = "( $imm )";
  let InOperandList = (ins port8:$imm);
}

def HRAMMode : AddressingMode {
  let OperandsStr = "( $imm )";
  let InOperandList = (ins hram8:$imm);
}

def SImm8Mode : AddressingMode {
  let OperandsStr = "$imm";
  let InOperandList = (ins simm8:$imm);
}

//===----------------------------------------------------------------------===//
//  Unprefixed instruction formats
//===----------------------------------------------------------------------===//

/// 1-byte unprefixed instruction (NOP, HALT, RET, etc.)
class Inst8<string asmstr, bits<8> opcode> : Z80Inst8<asmstr> {
  let Inst = opcode;
  let InOperandList = (ins);
}

/// 2-byte unprefixed instruction: opcode and an 8-bit operand
class Inst16<string asmstr, bits<8> opcode, AddressingMode mode>
    : Z80Inst16<asmstr> {
  bits<8> imm;
  let Inst{7-0} = opcode;
  let Inst{15-8} = imm;
  let InOperandList = mode.InOperandList;
  let AsmString = asmstr # " " # mode.OperandsStr;
}

/// 3-byte unprefixed instruction: opcode and a 16-bit operand
class Inst24<string asmstr, bits<8> opcode, AddressingMode mode>
    : Z80Inst24<asmstr> {
  bits<16> imm;
  let Inst{7-0} = opcode;
  let Inst{23-8} = imm;
  let InOperandList = mode.InOperandList;
  let AsmString = asmstr # " " # mode.OperandsStr;
}

//===----------------------------------------------------------------------===//
//  CB-prefixed instructions (bit manipulation)
//===----------------------------------------------------------------------===//

/// 2-byte CB-prefixed instruction
class InstCB<string asmstr, bits<8> opcode> : Z80Inst16<asmstr> {
  let Inst{7-0} = 0xCB;
  let Inst{15-8} = opcode;
  let InOperandList = (ins);
}

//===----------------------------------------------------------------------===//
//  DD-prefixed instructions (IX operations)
//===----------------------------------------------------------------------===//

/// 2-byte DD-prefixed instruction
class InstDD<string asmstr, bits<8> opcode> : Z80Inst16<asmstr> {
  let Inst{7-0} = 0xDD;
  let Inst{15-8} = opcode;
  let InOperandList = (ins);
}

/// 3-byte DD-prefixed instruction with 8-bit operand (IX+d addressing)
class InstDDd<string asmstr, bits<8> opcode>
    : Z80Inst24<asmstr # " ( ix + $imm )"> {
  bits<8> imm;
  let Inst{7-0} = 0xDD;
  let Inst{15-8} = opcode;
  let Inst{23-16} = imm;
  let InOperandList = (ins disp8:$imm);
}

/// 4-byte DD-prefixed instruction with displacement and 8-bit operand
class InstDDdn<string asmstr, bits<8> opcode>
    : Z80Inst32<asmstr # " ( ix + $disp ), $val"> {
  bits<8> disp;
  bits<8> val;
  let Inst{7-0} = 0xDD;
  let Inst{15-8} = opcode;
  let Inst{23-16} = disp;
  let Inst{31-24} = val;
  let InOperandList = (ins disp8:$disp, imm8at3:$val);
}

/// 4-byte DD-prefixed instruction with 16-bit operand
class InstDDnn<string asmstr, bits<8> opcode, AddressingMode mode>
    : Z80Inst32<asmstr> {
  bits<16> imm;
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
class InstFD<string asmstr, bits<8> opcode> : Z80Inst16<asmstr> {
  let Inst{7-0} = 0xFD;
  let Inst{15-8} = opcode;
  let InOperandList = (ins);
}

/// 3-byte FD-prefixed instruction with 8-bit operand (IY+d addressing)
class InstFDd<string asmstr, bits<8> opcode>
    : Z80Inst24<asmstr # " ( iy + $imm )"> {
  bits<8> imm;
  let Inst{7-0} = 0xFD;
  let Inst{15-8} = opcode;
  let Inst{23-16} = imm;
  let InOperandList = (ins disp8:$imm);
}

/// 4-byte FD-prefixed instruction with displacement and 8-bit operand
class InstFDdn<string asmstr, bits<8> opcode>
    : Z80Inst32<asmstr # " ( iy + $disp ), $val"> {
  bits<8> disp;
  bits<8> val;
  let Inst{7-0} = 0xFD;
  let Inst{15-8} = opcode;
  let Inst{23-16} = disp;
  let Inst{31-24} = val;
  let InOperandList = (ins disp8:$disp, imm8at3:$val);
}

/// 4-byte FD-prefixed instruction with 16-bit operand
class InstFDnn<string asmstr, bits<8> opcode, AddressingMode mode>
    : Z80Inst32<asmstr> {
  bits<16> imm;
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
class InstED<string asmstr, bits<8> opcode> : Z80Inst16<asmstr> {
  let Inst{7-0} = 0xED;
  let Inst{15-8} = opcode;
  let InOperandList = (ins);
}

/// 4-byte ED-prefixed instruction with 16-bit operand.
class InstEDnn<string asmstr, bits<8> opcode, AddressingMode mode>
    : Z80Inst32<asmstr> {
  bits<16> imm;
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
class InstDDCB<string asmstr, bits<8> opcode>
    : Z80Inst32<asmstr # " ( ix + $imm )"> {
  bits<8> imm;
  let Inst{7-0} = 0xDD;
  let Inst{15-8} = 0xCB;
  let Inst{23-16} = imm;
  let Inst{31-24} = opcode;
  let InOperandList = (ins disp8:$imm);
}

/// 4-byte FD CB indexed bit instruction
class InstFDCB<string asmstr, bits<8> opcode>
    : Z80Inst32<asmstr # " ( iy + $imm )"> {
  bits<8> imm;
  let Inst{7-0} = 0xFD;
  let Inst{15-8} = 0xCB;
  let Inst{23-16} = imm;
  let Inst{31-24} = opcode;
  let InOperandList = (ins disp8:$imm);
}

//===----------------------------------------------------------------------===//
//  Condition code encoding
//===----------------------------------------------------------------------===//

// Z80 condition codes
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
