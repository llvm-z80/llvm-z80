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

let Predicates = [IsSM83], DecoderNamespace = "SM83" in {

//===----------------------------------------------------------------------===//
//  Hardware Instructions
//===----------------------------------------------------------------------===//

// SWAP r / SWAP (HL)
let mayLoad = 0, mayStore = 0, hasSideEffects = 0 in
def SWAP_r : CBRotate<"swap", 0b110>;
let mayLoad = true, mayStore = true, hasSideEffects = false in
def SWAP_HLind : InstCB<"swap (hl)", 0x36> { let Uses = [HL]; let Defs = [F]; }

// SM83 RETI - Return from interrupt (opcode 0xD9)
// Unlike Z80 RETI (ED 4D), SM83 RETI is a single-byte instruction
// that atomically enables interrupts (IME=1) and returns.
// No conflict: SM83 DecoderNamespace separates from Z80's EXX (also 0xD9).
let isReturn = true, isTerminator = true, isBarrier = true in
def SM83_RETI : Inst8<"reti", 0xD9>;

// LDHL SP,e - Load SP + signed 8-bit offset into HL (opcode 0xF8)
// sdasgb syntax: ldhl sp,#imm
def LDHL_SP_e : Inst16<"ldhl sp,", 0xF8, SImm8Mode> {
  let Uses = [SP];
  let Defs = [HL, F];
  let mayLoad = 0;
  let mayStore = 0;
  let hasSideEffects = 0;
}

// ADD SP,e - Add signed 8-bit immediate to SP (opcode 0xE8)
// sdasgb syntax: add sp,#imm
def ADD_SP_e : Inst16<"add sp,", 0xE8, SImm8Mode> {
  let Uses = [SP];
  let Defs = [SP, F];
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

// LD (nn),SP
let mayLoad = false, mayStore = true, hasSideEffects = false in
def SM83_LD_nnind_SP : Inst24<"ld", 0x08, DirectMode> {
  let AsmString = "ld ( $imm ),sp";
  let Uses = [SP];
}

// STOP
def STOP : Z80Inst16<"stop"> {
  let Inst{7-0} = 0x10;
  let Inst{15-8} = 0x00;
  let InOperandList = (ins);
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

// Indirect call through HL via __call_hl trampoline.
def CALL_HL : Z80Pseudo<(outs), (ins)> {
  let isCall = true;
  let Uses = [HL];
  let Defs = [A, BC, DE, HL, F];
  let mayLoad = true;
  let mayStore = true;
  let hasSideEffects = true;
}

//===----------------------------------------------------------------------===//
//  LDH Instructions (0xFF00 + 8-bit offset)
//===----------------------------------------------------------------------===//

// LDH (n),A - Store A to high page address 0xFF00+n (opcode 0xE0)
let mayLoad = false, mayStore = true, hasSideEffects = true in
def SM83_LDH_nind_A : Inst16<"ldh", 0xE0, HRAMMode> {
  let AsmString = "ldh ( $imm ), a";
  let Uses = [A];
}

// LDH A,(n) - Load A from high page address 0xFF00+n (opcode 0xF0)
let mayLoad = true, mayStore = false, hasSideEffects = true in
def SM83_LDH_A_nind : Inst16<"ldh a,", 0xF0, HRAMMode> {
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

} // Predicates = [IsSM83]
