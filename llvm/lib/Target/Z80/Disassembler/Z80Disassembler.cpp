//===- Z80Disassembler.cpp - Disassembler for Z80 ---------------*- C++ -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file is part of the Z80 Disassembler.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80.h"
#include "Z80Subtarget.h"

#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDecoder.h"
#include "llvm/MC/MCDecoderOps.h"
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;
using namespace llvm::MCD;

#define DEBUG_TYPE "z80-disassembler"

using DecodeStatus = MCDisassembler::DecodeStatus;

namespace {
/// A disassembler class for Z80.
class Z80Disassembler : public MCDisassembler {
public:
  Z80Disassembler(const MCSubtargetInfo &STI, MCContext &Ctx)
      : MCDisassembler(STI, Ctx) {}

  DecodeStatus getInstruction(MCInst &Instr, uint64_t &Size,
                              ArrayRef<uint8_t> Bytes, uint64_t Address,
                              raw_ostream &CStream) const override;
};
} // namespace

MCDisassembler *createZ80Disassembler(const Target &T,
                                      const MCSubtargetInfo &STI,
                                      MCContext &Ctx) {
  return new Z80Disassembler(STI, Ctx);
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeZ80Disassembler() {
  // Register the disassembler for both Z80 and SM83 targets.
  TargetRegistry::RegisterMCDisassembler(getTheZ80Target(),
                                         createZ80Disassembler);
  TargetRegistry::RegisterMCDisassembler(getTheSM83Target(),
                                         createZ80Disassembler);
}

template <unsigned N>
static DecodeStatus decodeUImmOperand(MCInst &Inst, uint64_t Imm,
                                      int64_t Address,
                                      const MCDisassembler *Decoder) {
  if (!isUInt<N>(Imm))
    return MCDisassembler::Fail;
  Inst.addOperand(MCOperand::createImm(Imm));
  return MCDisassembler::Success;
}

template <unsigned N>
static DecodeStatus decodeSImmOperand(MCInst &Inst, uint64_t Imm,
                                      int64_t Address,
                                      const MCDisassembler *Decoder) {
  if (!isUInt<N>(Imm))
    return MCDisassembler::Fail;
  Inst.addOperand(MCOperand::createImm(SignExtend64<N>(Imm)));
  return MCDisassembler::Success;
}

#include "Z80GenDisassemblerTables.inc"

DecodeStatus Z80Disassembler::getInstruction(MCInst &Instr, uint64_t &Size,
                                             ArrayRef<uint8_t> Bytes,
                                             uint64_t Address,
                                             raw_ostream &CStream) const {
  Size = 0;
  bool IsSM83 = STI.hasFeature(Z80::FeatureSM83);

  // Try decoding with increasing instruction sizes (1 to 4 bytes).
  // Z80 instructions are 1-4 bytes long.
  for (size_t InsnSize : {1, 2, 3, 4}) {
    if (Bytes.size() < InsnSize)
      return MCDisassembler::Fail;

    uint64_t Insn = 0;
    for (size_t Byte = 0; Byte < InsnSize; ++Byte)
      Insn |= ((uint64_t)Bytes[Byte]) << (8 * Byte);

    // SM83 has its own DecoderNamespace for instructions that reuse Z80
    // opcode slots (e.g., 0x22/0x2A/0x32/0x3A/0xD9/0xEA/0xFA).
    // Try the SM83-specific tables first when targeting SM83.
    if (IsSM83) {
      const uint8_t *SM83Table = nullptr;
      switch (InsnSize) {
      case 1:
        SM83Table = DecoderTableSM838;
        break;
      case 2:
        SM83Table = DecoderTableSM8316;
        break;
      case 3:
        SM83Table = DecoderTableSM8324;
        break;
      }
      if (SM83Table) {
        DecodeStatus Result =
            decodeInstruction(SM83Table, Instr, Insn, Address, this, STI);
        if (Result != MCDisassembler::Fail) {
          Size = InsnSize;
          return Result;
        }
      }
    }

    // Common + Z80-specific tables (predicate-gated).
    const uint8_t *Table = nullptr;
    switch (InsnSize) {
    case 1:
      Table = DecoderTableZ808;
      break;
    case 2:
      Table = DecoderTableZ8016;
      break;
    case 3:
      Table = DecoderTableZ8024;
      break;
    case 4:
      Table = DecoderTableZ8032;
      break;
    }

    DecodeStatus Result =
        decodeInstruction(Table, Instr, Insn, Address, this, STI);
    if (Result != MCDisassembler::Fail) {
      Size = InsnSize;
      return Result;
    }
  }

  return MCDisassembler::Fail;
}
