//===-- Z80InstPrinter.cpp - Convert Z80 MCInst to assembly syntax --------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This class prints a Z80 MCInst to a .s file.
//
//===----------------------------------------------------------------------===//

#include "Z80InstPrinter.h"

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80MCExpr.h"

#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrDesc.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/raw_ostream.h"

#include <cstring>
#include <sstream>

#define DEBUG_TYPE "asm-printer"

namespace llvm {

void Z80InstPrinter::printInst(const MCInst *MI, uint64_t Address,
                               StringRef Annot, const MCSubtargetInfo &STI,
                               raw_ostream &OS) {
  std::string AiryOperands;
  raw_string_ostream AiryOperandStream(AiryOperands);
  assert(getMnemonic(*MI).second && "Missing opcode for instruction.");
  printInstruction(MI, Address, AiryOperandStream);
  AiryOperands = AiryOperandStream.str();
  size_t SpacesSeen = 0;
  std::string CorrectOperands;
  for (const auto &Letter : AiryOperands) {
    if (isspace(Letter) != 0) {
      if (++SpacesSeen <= 2) {
        CorrectOperands += '\t';
      }
      continue;
    }
    CorrectOperands += Letter;
  }
  OS << CorrectOperands;
}

void Z80InstPrinter::printOperand(const MCInst *MI, unsigned OpNo,
                                  raw_ostream &OS) {
  const MCOperand &Op = MI->getOperand(OpNo);

  if (Op.isReg()) {
    printRegName(OS, Op.getReg());
  } else if (Op.isImm()) {
    OS << formatImm(Op.getImm());
  } else {
    assert(Op.isExpr() && "Unknown operand kind in printOperand");
    // Format z80_16 immediates using formatImm.
    if (const auto *MME = dyn_cast<Z80MCExpr>(Op.getExpr())) {
      int64_t Value = 0;
      if (MME->getKind() == Z80MCExpr::VK_IMM16 &&
          MME->getSubExpr()->evaluateAsAbsolute(Value)) {
        OS << "z80_16(" << formatImm(Value) << ')';
        return;
      }
    }
    MAI.printExpr(OS, *Op.getExpr());
  }
}

void Z80InstPrinter::printBranchOperand(const MCInst *MI, uint64_t Address,
                                        unsigned OpNo, raw_ostream &OS) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (!Op.isImm())
    return printOperand(MI, OpNo, OS);
  uint64_t Target = Op.getImm();
  OS << formatImm(PrintBranchImmAsAddress ? (int8_t)Target + Address + 2
                                          : Target);
}

void Z80InstPrinter::printIndexedOperand(const MCInst *MI, unsigned OpNo,
                                         raw_ostream &OS) {
  // Print indexed addressing operand displacement
  // The AsmString already includes "( ix + " or "( iy + ", so we just print
  // the displacement value. For negative values, we print the absolute value
  // since we need to change "ix + -3" to "ix - 3".
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isImm()) {
    int64_t Disp = Op.getImm();
    OS << formatImm(Disp);
  } else if (Op.isExpr()) {
    MAI.printExpr(OS, *Op.getExpr());
  }
}

void Z80InstPrinter::printRegName(raw_ostream &OS, MCRegister Reg) {
  OS << getRegisterName(Reg);
}

format_object<int64_t> Z80InstPrinter::formatHex(int64_t Value) const {
  switch (PrintHexStyle) {
  case HexStyle::C:
    if (Value < 0) {
      return format("-$%" PRIx64, -Value);
    } else {
      return format("$%" PRIx64, Value);
    }
  case HexStyle::Asm:
    if (Value < 0) {
      return format("-$%" PRIx64, -Value);
    } else {
      return format("$%" PRIx64, Value);
    }
  }
  llvm_unreachable("unsupported print style");
}

format_object<uint64_t> Z80InstPrinter::formatHex(uint64_t Value) const {
  switch (PrintHexStyle) {
  case HexStyle::C:
    return format("$%" PRIx64, Value);
  case HexStyle::Asm:
    return format("$%" PRIx64, Value);
  }
  llvm_unreachable("unsupported print style");
}

// Include the auto-generated portion of the assembly writer.
#define PRINT_ALIAS_INSTR
#include "Z80GenAsmWriter.inc"

//===----------------------------------------------------------------------===//
// Z80InstPrinterSDCC - sdasz80 compatible instruction printing
//===----------------------------------------------------------------------===//

/// Convert indexed addressing from Zilog format to sdasz80 format.
/// Transforms (ix+N) → N(ix), (ix+-N) → -N(ix), same for iy.
static std::string convertIndexedAddressing(const std::string &Input) {
  std::string Result;
  size_t i = 0;
  while (i < Input.size()) {
    // Look for "(ix+" or "(ix-" or "(iy+" or "(iy-"
    if (i + 4 <= Input.size() && Input[i] == '(' && Input[i + 1] == 'i' &&
        (Input[i + 2] == 'x' || Input[i + 2] == 'y') &&
        (Input[i + 3] == '+' || Input[i + 3] == '-')) {
      char Reg = Input[i + 2]; // 'x' or 'y'
      // Find the closing ')'
      size_t Close = Input.find(')', i + 4);
      if (Close != std::string::npos) {
        // Extract displacement (everything after the sign)
        std::string Disp = Input.substr(i + 3, Close - (i + 3));
        // Handle "+-N" → "-N", "+N" → "N"
        if (Disp.size() >= 2 && Disp[0] == '+' && Disp[1] == '-')
          Disp = Disp.substr(1);
        else if (!Disp.empty() && Disp[0] == '+')
          Disp = Disp.substr(1);
        // Output: disp(ix) or disp(iy)
        Result += Disp;
        Result += "(i";
        Result += Reg;
        Result += ')';
        i = Close + 1;
        continue;
      }
    }
    Result += Input[i++];
  }
  return Result;
}

void Z80InstPrinterSDCC::printInst(const MCInst *MI, uint64_t Address,
                                   StringRef Annot, const MCSubtargetInfo &STI,
                                   raw_ostream &OS) {
  std::string AiryOperands;
  raw_string_ostream AiryOperandStream(AiryOperands);
  assert(getMnemonic(*MI).second && "Missing opcode for instruction.");
  printInstruction(MI, Address, AiryOperandStream);
  AiryOperands = AiryOperandStream.str();

  // Normalize whitespace (same as base class)
  size_t SpacesSeen = 0;
  std::string Normalized;
  for (const auto &Letter : AiryOperands) {
    if (isspace(Letter) != 0) {
      if (++SpacesSeen <= 2)
        Normalized += '\t';
      continue;
    }
    Normalized += Letter;
  }

  // Convert indexed addressing: (ix+N) → N(ix), (iy+N) → N(iy)
  OS << convertIndexedAddressing(Normalized);
}

void Z80InstPrinterSDCC::printOperand(const MCInst *MI, unsigned OpNo,
                                      raw_ostream &OS) {
  const MCOperand &Op = MI->getOperand(OpNo);

  // Check if this operand is an immediate that needs # prefix in sdasz80.
  // Branch/call targets should NOT get # even if typed as IMM16.
  bool NeedsHash = false;
  const MCInstrDesc &Desc = MII.get(MI->getOpcode());
  if (OpNo < Desc.getNumOperands() && !Desc.isCall() && !Desc.isBranch()) {
    uint8_t OpType = Desc.operands()[OpNo].OperandType;
    NeedsHash =
        (OpType == Z80Op::OPERAND_IMM8 || OpType == Z80Op::OPERAND_IMM16);
  }

  if (Op.isReg()) {
    printRegName(OS, Op.getReg());
  } else if (Op.isImm()) {
    if (NeedsHash)
      OS << '#';
    OS << formatImm(Op.getImm());
  } else {
    assert(Op.isExpr() && "Unknown operand kind in printOperand");
    if (NeedsHash)
      OS << '#';
    // Format z80_16 immediates using formatImm.
    if (const auto *MME = dyn_cast<Z80MCExpr>(Op.getExpr())) {
      int64_t Value = 0;
      if (MME->getKind() == Z80MCExpr::VK_IMM16 &&
          MME->getSubExpr()->evaluateAsAbsolute(Value)) {
        OS << "z80_16(" << formatImm(Value) << ')';
        return;
      }
    }
    MAI.printExpr(OS, *Op.getExpr());
  }
}

format_object<int64_t> Z80InstPrinterSDCC::formatHex(int64_t Value) const {
  if (Value < 0)
    return format("-0x%" PRIx64, -Value);
  return format("0x%" PRIx64, Value);
}

format_object<uint64_t> Z80InstPrinterSDCC::formatHex(uint64_t Value) const {
  return format("0x%" PRIx64, Value);
}

} // end of namespace llvm
