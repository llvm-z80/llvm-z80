//===-- Z80ELFObjectWriter.cpp - Z80 ELF Writer ---------------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Z80ELFObjectWriter.h"

#include "MCTargetDesc/Z80FixupKinds.h"
#include "MCTargetDesc/Z80MCExpr.h"
#include "MCTargetDesc/Z80MCTargetDesc.h"

#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSection.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/ErrorHandling.h"

namespace llvm {

Z80ELFObjectWriter::Z80ELFObjectWriter(uint8_t OSABI)
    : MCELFObjectTargetWriter(false, OSABI, ELF::EM_Z80, true) {}

unsigned Z80ELFObjectWriter::getRelocType(const MCFixup &Fixup,
                                          const MCValue &Target,
                                          bool IsPCRel) const {
  unsigned Kind = Fixup.getKind();
  auto Specifier = static_cast<Z80MCExpr::VariantKind>(Target.getSpecifier());
  switch (Kind) {
  case FK_Data_1:
    switch (Specifier) {
    default:
      llvm_unreachable("Unsupported Specifier");
    case Z80MCExpr::VK_NONE:
    case Z80MCExpr::VK_ADDR8:
      return ELF::R_Z80_ADDR8;
    case Z80MCExpr::VK_ADDR16_LO:
      return ELF::R_Z80_ADDR16_LO;
    case Z80MCExpr::VK_ADDR16_HI:
      return ELF::R_Z80_ADDR16_HI;
    case Z80MCExpr::VK_ADDR24_BANK:
      return ELF::R_Z80_ADDR24_BANK;
    case Z80MCExpr::VK_ADDR24_SEGMENT_LO:
      return ELF::R_Z80_ADDR24_SEGMENT_LO;
    case Z80MCExpr::VK_ADDR24_SEGMENT_HI:
      return ELF::R_Z80_ADDR24_SEGMENT_HI;
    case Z80MCExpr::VK_ADDR13:
      return ELF::R_Z80_ADDR13;
    }
  case FK_Data_2:
    switch (Specifier) {
    default:
      llvm_unreachable("Unsupported Specifier");
    case Z80MCExpr::VK_NONE:
    case Z80MCExpr::VK_ADDR16:
      return ELF::R_Z80_ADDR16;
    case Z80MCExpr::VK_ADDR13:
      return ELF::R_Z80_ADDR13;
    case Z80MCExpr::VK_ADDR24_SEGMENT:
      return ELF::R_Z80_ADDR24_SEGMENT;
    }

  case Z80::Imm8:
    return ELF::R_Z80_IMM8;
  case Z80::Addr8:
    return ELF::R_Z80_ADDR8;
  case Z80::Addr16:
    return ELF::R_Z80_ADDR16;
  case Z80::Addr16_Low:
    return ELF::R_Z80_ADDR16_LO;
  case Z80::Addr16_High:
    return ELF::R_Z80_ADDR16_HI;
  case Z80::Addr24:
    return ELF::R_Z80_ADDR24;
  case Z80::Addr24_Bank:
    return ELF::R_Z80_ADDR24_BANK;
  case Z80::Addr24_Segment:
    return ELF::R_Z80_ADDR24_SEGMENT;
  case Z80::Addr24_Segment_Low:
    return ELF::R_Z80_ADDR24_SEGMENT_LO;
  case Z80::Addr24_Segment_High:
    return ELF::R_Z80_ADDR24_SEGMENT_HI;
  case Z80::PCRel8:
    return ELF::R_Z80_PCREL_8;
  case Z80::PCRel16:
    return ELF::R_Z80_PCREL_16;
  case FK_Data_4:
    return ELF::R_Z80_FK_DATA_4;
  case FK_Data_8:
    return ELF::R_Z80_FK_DATA_8;
  case Z80::AddrAsciz:
    return ELF::R_Z80_ADDR_ASCIZ;
  case Z80::Imm16:
    return ELF::R_Z80_IMM16;
  case Z80::Addr13:
    return ELF::R_Z80_ADDR13;
  case Z80::Disp8:
    return ELF::R_Z80_DISP8;
  case Z80::LDH8:
    return ELF::R_SM83_LDH8;

  default:
    llvm_unreachable("invalid fixup kind!");
  }
}

std::unique_ptr<MCObjectTargetWriter> createZ80ELFObjectWriter(uint8_t OSABI) {
  return std::make_unique<Z80ELFObjectWriter>(OSABI);
}

} // end of namespace llvm
