//===-- Z80ELFObjectWriter.cpp - Z80 ELF Writer ---------------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/Z80FixupKinds.h"
#include "MCTargetDesc/Z80MCTargetDesc.h"

#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSection.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/ErrorHandling.h"

namespace llvm {

/// Writes Z80 machine code into an ELF32 object file.
class Z80ELFObjectWriter : public MCELFObjectTargetWriter {
public:
  explicit Z80ELFObjectWriter(uint8_t OSABI);
  virtual ~Z80ELFObjectWriter() = default;
  unsigned getRelocType(const MCFixup &Fixup, const MCValue &Target,
                        bool IsPCRel) const override;
};

} // end of namespace llvm
