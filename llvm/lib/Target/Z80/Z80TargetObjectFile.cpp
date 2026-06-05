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
#include "llvm/MC/SectionKind.h"

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
