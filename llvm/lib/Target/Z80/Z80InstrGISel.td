//===-- Z80InstrGISel.td - Z80 Generic Instructions --------*- tablegen -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Z80 Generic Instruction Definitions for GlobalISel.
//
//===----------------------------------------------------------------------===//

class Z80GenericInstruction : GenericInstruction {
  let Namespace = "Z80";
  let hasSideEffects = false;
}

//===---------------------------------------------------------------------===//
// Z80-specific comparison and branch operations
//===---------------------------------------------------------------------===//

// Compare two 8-bit values and branch based on condition.
// Used to fuse G_ICMP + G_BRCOND into a single operation.
def G_Z80_CMP_BR_EQ : Z80GenericInstruction {
  let OutOperandList = (outs);
  let InOperandList = (ins type0:$lhs, type0:$rhs, unknown:$target);
  let isBranch = true;
  let isTerminator = true;
}

def G_Z80_CMP_BR_NE : Z80GenericInstruction {
  let OutOperandList = (outs);
  let InOperandList = (ins type0:$lhs, type0:$rhs, unknown:$target);
  let isBranch = true;
  let isTerminator = true;
}

def G_Z80_CMP_BR_ULT : Z80GenericInstruction {
  let OutOperandList = (outs);
  let InOperandList = (ins type0:$lhs, type0:$rhs, unknown:$target);
  let isBranch = true;
  let isTerminator = true;
}

def G_Z80_CMP_BR_UGE : Z80GenericInstruction {
  let OutOperandList = (outs);
  let InOperandList = (ins type0:$lhs, type0:$rhs, unknown:$target);
  let isBranch = true;
  let isTerminator = true;
}

// 32-bit integer comparison via 8-bit SUB/SBC chain.
// Avoids the expensive narrowScalar cascade (2x G_ICMP + G_SELECT).
// The legalizer splits the i32 into two i16 halves and emits this.
// The instruction selector chains CMP16_FLAGS + CMP16_SBC_FLAGS.
def G_Z80_ICMP32 : Z80GenericInstruction {
  let OutOperandList = (outs type0:$dst);
  let InOperandList = (ins i32imm:$pred,
                           type1:$lhs_lo, type1:$lhs_hi,
                           type1:$rhs_lo, type1:$rhs_hi);
}

// 64-bit integer comparison via chained 16-bit SUB/SBC.
// The legalizer splits the i64 into four i16 words and emits this.
// Keeps register pressure low by processing one word pair at a time.
def G_Z80_ICMP64 : Z80GenericInstruction {
  let OutOperandList = (outs type0:$dst);
  let InOperandList = (ins i32imm:$pred,
                           type1:$lhs_w0, type1:$lhs_w1,
                           type1:$lhs_w2, type1:$lhs_w3,
                           type1:$rhs_w0, type1:$rhs_w1,
                           type1:$rhs_w2, type1:$rhs_w3);
}
