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
  // Use R_Z80_8_PCREL for a PC-relative .byte and reject the rest.
  if (IsPCRel && Kind != Z80::PCRel8) {
    if (Kind == FK_Data_1 && Specifier == Z80MCExpr::VK_NONE)
      return ELF::R_Z80_8_PCREL;
    reportError(Fixup.getLoc(), "unsupported PC-relative relocation");
    return ELF::R_Z80_NONE;
  }
  // For z80_16hi(x)+1, R_Z80_BYTE1 adds 1 to x before taking the high byte.
  // So reject a byte operator inside a larger expression.
  if (Specifier != Z80MCExpr::VK_NONE && !isa<Z80MCExpr>(Fixup.getValue())) {
    reportError(Fixup.getLoc(),
                "z80_16lo and z80_16hi must enclose the whole expression");
    return ELF::R_Z80_NONE;
  }
  // A wider field gets the byte zero-extended.
  if (Specifier == Z80MCExpr::VK_ADDR16_LO)
    return ELF::R_Z80_BYTE0;
  if (Specifier == Z80MCExpr::VK_ADDR16_HI)
    return ELF::R_Z80_BYTE1;
  switch (Kind) {
  case FK_Data_1:
  case Z80::Imm8:
  case Z80::Addr8:
  case Z80::LDH8:
    return ELF::R_Z80_8;
  case FK_Data_2:
  case Z80::Imm16:
  case Z80::Addr16:
    return ELF::R_Z80_16;
  case Z80::Addr16_Low:
    return ELF::R_Z80_BYTE0;
  case Z80::Addr16_High:
    return ELF::R_Z80_BYTE1;
  case Z80::PCRel8:
    return ELF::R_Z80_8_PCREL;
  case Z80::Disp8:
    return ELF::R_Z80_8_DIS;
  case FK_Data_4:
    return ELF::R_Z80_32;
  case FK_Data_8:
    reportError(Fixup.getLoc(), "8-byte data relocations are not supported");
    return ELF::R_Z80_NONE;

  default:
    llvm_unreachable("invalid fixup kind!");
  }
}

std::unique_ptr<MCObjectTargetWriter> createZ80ELFObjectWriter(uint8_t OSABI) {
  return std::make_unique<Z80ELFObjectWriter>(OSABI);
}

} // end of namespace llvm
