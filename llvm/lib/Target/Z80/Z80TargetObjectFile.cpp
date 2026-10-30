//===-- Z80TargetObjectFile.cpp - Z80 Object Files ------------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Z80TargetObjectFile.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/IR/GlobalObject.h"
#include "llvm/IR/Mangler.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/SectionKind.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"

using namespace llvm;

MCSection *Z80TargetObjectFile::getExplicitSectionGlobal(
    const GlobalObject *GO, SectionKind SK, const TargetMachine &TM) const {
  StringRef SectionName = GO->getSection();
  if (SectionName.ends_with(".noinit") || SectionName.contains(".noinit."))
    SK = SectionKind::getBSS();
  return TargetLoweringObjectFileELF::getExplicitSectionGlobal(GO, SK, TM);
}

// z80asm treats '.' as an operator. Encode each dot-separated part of the
// already-prefixed name as <length>_<part> prefixed with '_'.
static void encodeDottedName(SmallVectorImpl<char> &Name) {
  StringRef Original(Name.data(), Name.size());
  if (!Original.contains('.'))
    return;

  SmallString<128> Encoded("_");
  SmallVector<StringRef, 4> Parts;
  Original.split(Parts, '.');
  raw_svector_ostream OS(Encoded);
  for (StringRef Part : Parts)
    OS << Part.size() << '_' << Part;
  Name.assign(Encoded.begin(), Encoded.end());
}

// sdas accepts only letters, digits, `.`, `_`, and `$` in symbol names.
// Write every other byte as `$` plus two hex digits, so `(` becomes `$28`.
// `$` itself becomes `$24`, which keeps distinct names distinct.
static void escapeSDCCName(SmallVectorImpl<char> &Name, const MCAsmInfo &MAI) {
  SmallString<128> Escaped;
  raw_svector_ostream OS(Escaped);
  for (char C : Name)
    if (C != '$' && MAI.isAcceptableChar(C))
      OS << C;
    else
      OS << '$' << format_hex_no_prefix((unsigned char)C, 2);
  Name.assign(Escaped.begin(), Escaped.end());
}

static void encodeName(SmallVectorImpl<char> &Name, const MCAsmInfo &MAI) {
  if (MAI.isZ88DK())
    encodeDottedName(Name);
  else if (MAI.isSDCC())
    escapeSDCCName(Name, MAI);
}

MCSymbol *Z80TargetObjectFile::getTargetSymbol(const GlobalValue *GV,
                                               const TargetMachine &TM) const {
  const MCAsmInfo &MAI = TM.getMCAsmInfo();
  if (!MAI.isZ88DK() && !MAI.isSDCC())
    return nullptr;

  SmallString<128> NameStr;
  TM.getNameWithPrefix(NameStr, GV, getMangler(), /*MayAlwaysUsePrivate=*/true);
  encodeName(NameStr, MAI);
  return getContext().getOrCreateSymbol(NameStr);
}

void Z80TargetObjectFile::getNameWithPrefix(SmallVectorImpl<char> &OutName,
                                            const GlobalValue *GV,
                                            const TargetMachine &TM) const {
  TargetLoweringObjectFileELF::getNameWithPrefix(OutName, GV, TM);
  encodeName(OutName, TM.getMCAsmInfo());
}
