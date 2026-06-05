//===-- Z80MCInstrAnalysis.cpp - Z80 instruction analysis -----------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the Z80 instruction analysis implementation.
//
//===----------------------------------------------------------------------===//

#include "Z80MCInstrAnalysis.h"
#include "Z80MCTargetDesc.h"
#include "llvm/MC/MCInstrDesc.h"
#include "llvm/MC/MCSubtargetInfo.h"

namespace llvm {

// Z80Op::OperandType is already defined in Z80MCTargetDesc.h

bool Z80MCInstrAnalysis::evaluateBranch(const MCInst &Inst, uint64_t Addr,
                                        uint64_t Size, uint64_t &Target) const {
  if ((!isBranch(Inst) && !isCall(Inst)) || isIndirectBranch(Inst))
    return false;
  unsigned NumOps = Inst.getNumOperands();
  if (NumOps == 0)
    return false;
  const auto &Op = Info->get(Inst.getOpcode()).operands()[NumOps - 1];
  switch (Op.OperandType) {
  case Z80Op::OPERAND_ADDR16: {
    // Z80 uses 16-bit addresses
    Target = Inst.getOperand(NumOps - 1).getImm() & 0xFFFF;
    return true;
  }
  case Z80Op::OPERAND_PCREL: {
    // PC-relative branch (JR instructions)
    Target = Addr + Size + Inst.getOperand(NumOps - 1).getImm();
    return true;
  }
  }
  return false;
}

std::optional<uint64_t> Z80MCInstrAnalysis::evaluateMemoryOperandAddress(
    const MCInst &Inst, const MCSubtargetInfo *STI, uint64_t Addr,
    uint64_t Size) const {
  unsigned NumOps = Inst.getNumOperands();
  // Assumption: Every opcode has only one memory operand.
  for (unsigned OpIdx = 0; OpIdx < NumOps; OpIdx++) {
    const auto &Op = Info->get(Inst.getOpcode()).operands()[OpIdx];
    switch (Op.OperandType) {
    case Z80Op::OPERAND_ADDR8: {
      // 8-bit address (I/O port or zero page)
      return Inst.getOperand(OpIdx).getImm() & 0xFF;
    }
    case Z80Op::OPERAND_ADDR16: {
      // 16-bit address
      return Inst.getOperand(OpIdx).getImm() & 0xFFFF;
    }
    }
  }
  return std::nullopt;
}

} //  namespace llvm
