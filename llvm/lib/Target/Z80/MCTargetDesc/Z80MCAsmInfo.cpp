//===-- Z80MCAsmInfo.cpp - Z80 asm properties -----------------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the declarations of the Z80MCAsmInfo properties.
//
//===----------------------------------------------------------------------===//

#include "Z80MCAsmInfo.h"
#include "MCTargetDesc/Z80MCExpr.h"
#include "Z80MCTargetDesc.h"

#include "llvm/MC/MCSection.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/TargetParser/Triple.h"

namespace llvm {

cl::opt<Z80AsmFormatTy> Z80AsmFormat(
    "z80-asm-format",
    cl::desc("Override Z80 assembly output format (default: auto from triple)"),
    cl::values(clEnumValN(Z80AsmFormat_ELF, "elf", "ELF/GNU style"),
               clEnumValN(Z80AsmFormat_SDASZ80, "sdasz80",
                          "SDCC sdasz80 compatible")));

static const MCAsmInfo::AtSpecifier AtSpecifiers[] = {
    {Z80MCExpr::VK_IMM8, "z80_imm8"},
    {Z80MCExpr::VK_IMM16, "z80_imm16"},
    {Z80MCExpr::VK_ADDR8, "z80_8"},
    {Z80MCExpr::VK_ADDR16, "z80_16"},
    {Z80MCExpr::VK_ADDR16_LO, "z80_16lo"},
    {Z80MCExpr::VK_ADDR16_HI, "z80_16hi"},
    {Z80MCExpr::VK_ADDR24, "z80_24"},
    {Z80MCExpr::VK_ADDR24_BANK, "z80_24bank"},
    {Z80MCExpr::VK_ADDR24_SEGMENT, "z80_24segment"},
    {Z80MCExpr::VK_ADDR24_SEGMENT_LO, "z80_24segmentlo"},
    {Z80MCExpr::VK_ADDR24_SEGMENT_HI, "z80_24segmenthi"},
    {Z80MCExpr::VK_ADDR13, "z80_13"},
};

Z80MCAsmInfo::Z80MCAsmInfo(const Triple &TT, const MCTargetOptions &Options) {
  // While the platform uses 2-byte pointers, the ELF files use 4-byte pointers
  // to convey banking information; this field is used, among others, by the
  // DWARF debug structures.
  CodePointerSize = 4;
  CalleeSaveStackSlotSize = 0;
  SeparatorString = "\n";
  CommentString = ";";
  UseMotorolaIntegers = true;
  // Maximum instruction length across all supported subtargets.
  MaxInstLength = 7;
  SupportsDebugInformation = true;

  initializeAtSpecifiers(AtSpecifiers);
}

unsigned Z80MCAsmInfo::getMaxInstLength(const MCSubtargetInfo *STI) const {
  if (!STI)
    return MaxInstLength;

  // Z80 max instruction length:
  // - Basic Z80: 4 bytes (prefix + opcode + 2 bytes operand)
  // - eZ80: 6 bytes (ADL prefix + DD/FD + CB + displacement + opcode + operand)
  if (STI->hasFeature(Z80::FeatureEZ80))
    return 6;
  return 4;
}

//===----------------------------------------------------------------------===//
// Z80MCAsmInfoSDCC - sdasz80 compatible assembly format
//===----------------------------------------------------------------------===//

Z80MCAsmInfoSDCC::Z80MCAsmInfoSDCC(const Triple &TT,
                                   const MCTargetOptions &Options) {
  CodePointerSize = 2;
  CalleeSaveStackSlotSize = 0;
  SeparatorString = "\n";
  CommentString = ";";
  MaxInstLength = 4;

  // sdasz80 dialect (SyntaxVariant 1)
  AssemblerDialect = 1;

  // Suppress ELF-specific directives
  HasDotTypeDotSizeDirective = false;
  HasSingleParameterDotFile = false;
  HasIdentDirective = false;

  // sdasz80 uses 0xFF hex format (not $FF Motorola style)
  UseMotorolaIntegers = false;

  // sdasz80 data directives
  Data8bitsDirective = "\t.db\t";
  Data16bitsDirective = "\t.dw\t";
  Data32bitsDirective = nullptr;
  Data64bitsDirective = nullptr;

  // sdasz80 uses .ds for zero-fill (not .zero)
  ZeroDirective = "\t.ds\t";

  // sdasz80 escape handling differs from GNU as; emit strings as .db bytes
  AsciiDirective = nullptr;
  AscizDirective = nullptr;

  // Labels
  GlobalDirective = "\t.globl\t";
  PrivateLabelPrefix = ".L";

  initializeAtSpecifiers(AtSpecifiers);
}

unsigned Z80MCAsmInfoSDCC::getMaxInstLength(const MCSubtargetInfo *STI) const {
  if (!STI)
    return MaxInstLength;
  if (STI->hasFeature(Z80::FeatureEZ80))
    return 6;
  return 4;
}

void Z80MCAsmInfoSDCC::printSwitchToSection(const MCSection &Section,
                                            uint32_t Subsection,
                                            const Triple &T,
                                            raw_ostream &OS) const {
  StringRef Name = Section.getName();

  // Map ELF section names to sdasz80 .area directives
  if (Name == ".text" || Name.starts_with(".text."))
    OS << "\t.area\t_CODE\n";
  else if (Name == ".data" || Name.starts_with(".data."))
    OS << "\t.area\t_DATA\n";
  else if (Name == ".bss" || Name.starts_with(".bss."))
    OS << "\t.area\t_BSS\n";
  else if (Name == ".rodata" || Name.starts_with(".rodata."))
    OS << "\t.area\t_CODE\n";
  // Silently ignore other sections (.note.GNU-stack, .comment, etc.)
}

} //  namespace llvm
