//===-- Z80MCAsmInfo.h - Z80 asm properties ---------------------*- C++ -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the declaration of the Z80MCAsmInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_Z80_ASM_INFO_H
#define LLVM_Z80_ASM_INFO_H

#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCAsmInfoELF.h"
#include "llvm/Support/CommandLine.h"

namespace llvm {

class Triple;

enum Z80AsmFormatTy { Z80AsmFormat_ELF = 0, Z80AsmFormat_SDASZ80 = 1 };
extern cl::opt<Z80AsmFormatTy> Z80AsmFormat;

/// Specifies the format of Z80 assembly files (ELF/GNU style).
class Z80MCAsmInfo : public MCAsmInfoELF {
public:
  explicit Z80MCAsmInfo(const Triple &TT, const MCTargetOptions &Options);

  unsigned getMaxInstLength(const MCSubtargetInfo *STI) const override;
};

/// Specifies the format of Z80 assembly files for sdasz80 (SDCC assembler).
class Z80MCAsmInfoSDCC : public MCAsmInfo {
public:
  explicit Z80MCAsmInfoSDCC(const Triple &TT, const MCTargetOptions &Options);

  unsigned getMaxInstLength(const MCSubtargetInfo *STI) const override;

  void printSwitchToSection(const MCSection &Section, uint32_t Subsection,
                            const Triple &T, raw_ostream &OS) const override;

  bool useCodeAlign(const MCSection &Sec) const override { return false; }
};

} // end namespace llvm

#endif // LLVM_Z80_ASM_INFO_H
