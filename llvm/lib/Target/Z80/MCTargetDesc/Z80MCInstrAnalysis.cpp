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

bool Z80MCInstrAnalysis::evaluateBranch(const MCInst &Inst, uint64_t Addr,
                                        uint64_t Size, uint64_t &Target) const {
  if ((!isBranch(Inst) && !isCall(Inst)) || isIndirectBranch(Inst))
    return false;
  unsigned NumOps = Inst.getNumOperands();
  if (NumOps == 0 || !Inst.getOperand(NumOps - 1).isImm())
    return false;
  int64_t Imm = Inst.getOperand(NumOps - 1).getImm();
  // JR and DJNZ take an offset from the next instruction. JP and CALL take an
  // address.
  if (Info->get(Inst.getOpcode()).operands()[NumOps - 1].OperandType ==
      MCOI::OPERAND_PCREL)
    Target = Addr + Size + Imm;
  else
    Target = Imm & 0xFFFF;
  return true;
}

std::optional<uint64_t> Z80MCInstrAnalysis::evaluateMemoryOperandAddress(
    const MCInst &Inst, const MCSubtargetInfo *STI, uint64_t Addr,
    uint64_t Size) const {
  const MCInstrDesc &Desc = Info->get(Inst.getOpcode());
  // Assumption: Every opcode has only one memory operand.
  for (unsigned OpIdx = 0, E = Desc.getNumOperands(); OpIdx != E; ++OpIdx)
    if (Desc.operands()[OpIdx].OperandType == MCOI::OPERAND_MEMORY &&
        Inst.getOperand(OpIdx).isImm())
      return Inst.getOperand(OpIdx).getImm() & 0xFFFF;
  return std::nullopt;
}

} //  namespace llvm
