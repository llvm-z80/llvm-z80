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
#include "llvm/Target/TargetMachine.h"

using namespace llvm;

void Z80TargetObjectFile::Initialize(MCContext &Ctx, const TargetMachine &TM) {
  TargetLoweringObjectFileELF::Initialize(Ctx, TM);
}

MCSection *Z80TargetObjectFile::getExplicitSectionGlobal(
    const GlobalObject *GO, SectionKind SK, const TargetMachine &TM) const {
  StringRef SectionName = GO->getSection();
  if (SectionName.ends_with(".noinit") || SectionName.contains(".noinit."))
    SK = SectionKind::getBSS();
  return TargetLoweringObjectFileELF::getExplicitSectionGlobal(GO, SK, TM);
}

// Sanitizes '.' characters in symbol names when generating assembly for z88dk's
// z80asm assembler.
//
// WHAT: Replaces all occurrences of '.' with '_' in symbol names.
// WHY:  In z80asm, '.' is a syntax token (used for local labels and floating-point
//       literals) and cannot appear inside an identifier. Dotted identifiers
//       (such as string constants or static locals) produce parse errors.
//
// Worked example:
//   - String literal: LLVM IR `@.str.1` mangled under Mach-O prefix (`m:o`) is
//     `L_.str.1`. Sanitization flattens it to `L__str_1`.
//   - Function-local static: `static int counter` inside `test_static` becomes
//     IR `@test_static.counter`, mangling to `_test_static.counter`.
//     Sanitization flattens it to `_test_static_counter`.
MCSymbol *Z80TargetObjectFile::getTargetSymbol(const GlobalValue *GV,
                                              const TargetMachine &TM) const {
  const MCAsmInfo &MAI = TM.getMCAsmInfo();
  // Only sanitize symbols when targeting z88dk z80asm assembly format
  if (!MAI.isZ80ASM())
    return nullptr;

  SmallString<128> NameStr;
  TM.getNameWithPrefix(NameStr, GV, getMangler(), /*MayAlwaysUsePrivate=*/true);
  for (char &C : NameStr) {
    if (C == '.')
      C = '_';
  }
  return getContext().getOrCreateSymbol(NameStr);
}

void Z80TargetObjectFile::getNameWithPrefix(SmallVectorImpl<char> &OutName,
                                           const GlobalValue *GV,
                                           const TargetMachine &TM) const {
  TargetLoweringObjectFileELF::getNameWithPrefix(OutName, GV, TM);
  const MCAsmInfo &MAI = TM.getMCAsmInfo();
  // Only sanitize symbols when targeting z88dk z80asm assembly format
  if (!MAI.isZ80ASM())
    return;

  for (char &C : OutName) {
    if (C == '.')
      C = '_';
  }
}
