//===-- SM83InstrInfo.td - SM83-only instructions ----------*- tablegen -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// SM83 (Game Boy CPU) instructions that do not exist on Z80.
// On Z80, these opcode slots are used for different instructions.
//
//===----------------------------------------------------------------------===//

let Predicates = [HasSM83], DecoderNamespace = "SM83" in {

//===----------------------------------------------------------------------===//
//  Hardware Instructions
//===----------------------------------------------------------------------===//

// SWAP A - Swap upper and lower nibbles of A (CB 37)
// On Z80, this opcode slot (CB 0b110rrr) is the undocumented SLL.
// SM83 uses it as a nibble swap instruction.
def SWAP_A : InstCB<"swap a", 0x37> {
  let Uses = [A];
  let Defs = [A, FLAGS];
}

// SM83 RETI - Return from interrupt (opcode 0xD9)
// Unlike Z80 RETI (ED 4D), SM83 RETI is a single-byte instruction
// that atomically enables interrupts (IME=1) and returns.
// No conflict: SM83 DecoderNamespace separates from Z80's EXX (also 0xD9).
def SM83_RETI : Inst8<"reti", 0xD9>, InstReturn;

// LDHL SP,e - Load SP + signed 8-bit offset into HL (opcode 0xF8)
// sdasgb syntax: ldhl sp,#imm
def LDHL_SP_e : Inst16SImm<"ldhl sp,", 0xF8> {
  let Uses = [SP];
  let Defs = [HL, FLAGS];
}

// ADD SP,e - Add signed 8-bit immediate to SP (opcode 0xE8)
// sdasgb syntax: add sp,#imm
def ADD_SP_e : Inst16SImm<"add sp,", 0xE8> {
  let Uses = [SP];
  let Defs = [SP, FLAGS];
}

// LD (nn),A - Store A to direct address (opcode 0xEA)
// Z80 uses 0x32 for this; SM83 0x32 is LD (HL-),A.
let mayLoad = false, mayStore = true, hasSideEffects = false in
def SM83_LD_nnind_A : Inst24<"ld", 0xEA, DirectMode> {
  let AsmString = "ld ( $imm ),a";
  let Uses = [A];
}

// LD A,(nn) - Load A from direct address (opcode 0xFA)
// Z80 uses 0x3A for this; SM83 0x3A is LD A,(HL-).
let mayLoad = true, mayStore = false, hasSideEffects = false in
def SM83_LD_A_nnind : Inst24<"ld a,", 0xFA, DirectMode> {
  let Defs = [A];
}

// LD (HL+),A - Store A to (HL) then increment HL (opcode 0x22)
// Z80 uses 0x22 for LD (nn),HL.
// sdasgb syntax: ld (hl+),a
def LD_HLI_A : Inst8<"ld (hl+), a", 0x22> {
  let Uses = [HL, A];
  let Defs = [HL];
  let mayLoad = false;
  let mayStore = true;
}

// LD A,(HL+) - Load (HL) to A then increment HL (opcode 0x2A)
// Z80 uses 0x2A for LD HL,(nn).
// sdasgb syntax: ld a,(hl+)
def LD_A_HLI : Inst8<"ld a, (hl+)", 0x2A> {
  let Uses = [HL];
  let Defs = [A, HL];
  let mayLoad = true;
  let mayStore = false;
}

// LD (HL-),A - Store A to (HL) then decrement HL (opcode 0x32)
// Z80 uses 0x32 for LD (nn),A.
// sdasgb syntax: ld (hl-),a
def LD_HLD_A : Inst8<"ld (hl-), a", 0x32> {
  let Uses = [HL, A];
  let Defs = [HL];
  let mayLoad = false;
  let mayStore = true;
}

// LD A,(HL-) - Load (HL) to A then decrement HL (opcode 0x3A)
// Z80 uses 0x3A for LD A,(nn).
// sdasgb syntax: ld a,(hl-)
def LD_A_HLD : Inst8<"ld a, (hl-)", 0x3A> {
  let Uses = [HL];
  let Defs = [A, HL];
  let mayLoad = true;
  let mayStore = false;
}

//===----------------------------------------------------------------------===//
//  Pseudo Instructions
//===----------------------------------------------------------------------===//

// SM83 16-bit equality comparison (sets Z flag directly).
// XOR both bytes, OR results — Z=1 if equal, Z=0 if not.
// Used in fused compare-and-branch (Z80 uses SUB_HL_rr instead).
def SM83_CMP_Z16 : Z80Pseudo<(outs), (ins GR16:$lhs, GR16:$rhs)> {
  let Defs = [A, B, FLAGS];
  let mayLoad = false;
  let mayStore = false;
}

// Lightweight 16-bit zero test: LD A,lo; OR hi — sets Z if reg==0.
// Only clobbers A (not B), reducing register pressure in loops.
def SM83_CMP_ZERO16 : Z80Pseudo<(outs), (ins GR16:$src)> {
  let Defs = [A, FLAGS];
  let mayLoad = false;
  let mayStore = false;
}

// SM83 signed 16-bit add with overflow detection.
// HL = HL + rr, A = overflow (0 or 1).
// SM83 has no P/V flag, so overflow is computed via sign bit comparison:
//   overflow = (result_hi ^ lhs_hi) & (result_hi ^ rhs_hi), bit 7
// Uses the "other" register pair (BC if rhs=DE, DE if rhs=BC) as temps.
def SM83_SADDO_HL_rr : Z80Pseudo<(outs), (ins GR16_BCDE:$rhs)> {
  let Defs = [A, BC, DE, HL, FLAGS];
  let Uses = [HL];
  let mayLoad = false;
  let mayStore = false;
}

// SM83 signed 16-bit sub with overflow detection.
// HL = HL - rr, A = overflow (0 or 1).
// overflow = (result_hi ^ lhs_hi) & (lhs_hi ^ rhs_hi), bit 7
def SM83_SSUBO_HL_rr : Z80Pseudo<(outs), (ins GR16_BCDE:$rhs)> {
  let Defs = [A, BC, DE, HL, FLAGS];
  let Uses = [HL];
  let mayLoad = false;
  let mayStore = false;
}

//===----------------------------------------------------------------------===//
//  LDH Instructions (0xFF00 + 8-bit offset)
//===----------------------------------------------------------------------===//

// LDH (n),A - Store A to high page address 0xFF00+n (opcode 0xE0)
let mayLoad = false, mayStore = true, hasSideEffects = true in
def SM83_LDH_nind_A : Z80Inst<"ldh"> {
  let Size = 2;
  bits<8> port;
  bits<16> Inst;
  let Inst{7-0} = 0xE0;
  let Inst{15-8} = port;
  let InOperandList = (ins hram8:$port);
  let AsmString = "ldh ( $port ), a";
  let Uses = [A];
}

// LDH A,(n) - Load A from high page address 0xFF00+n (opcode 0xF0)
let mayLoad = true, mayStore = false, hasSideEffects = true in
def SM83_LDH_A_nind : Z80Inst<"ldh a,"> {
  let Size = 2;
  bits<8> port;
  bits<16> Inst;
  let Inst{7-0} = 0xF0;
  let Inst{15-8} = port;
  let InOperandList = (ins hram8:$port);
  let AsmString = "ldh a, ( $port )";
  let Defs = [A];
}

// LDH (C),A - Store A to address 0xFF00+C (opcode 0xE2)
def SM83_LDH_Cind_A : Inst8<"ldh (c), a", 0xE2> {
  let Uses = [A, C];
  let mayLoad = false;
  let mayStore = true;
  let hasSideEffects = true;
}

// LDH A,(C) - Load from address 0xFF00+C to A (opcode 0xF2)
def SM83_LDH_A_Cind : Inst8<"ldh a, (c)", 0xF2> {
  let Uses = [C];
  let Defs = [A];
  let mayLoad = true;
  let mayStore = false;
  let hasSideEffects = true;
}

} // Predicates = [HasSM83]
