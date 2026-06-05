//===--- Z80.cpp - Implement Z80 target feature support -----------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements Z80 TargetInfo objects.
//
//===----------------------------------------------------------------------===//

#include "Z80.h"
#include "clang/Basic/MacroBuilder.h"
#include "clang/Basic/TargetInfo.h"

using namespace clang::targets;

Z80TargetInfo::Z80TargetInfo(const llvm::Triple &Triple, const TargetOptions &)
    : TargetInfo(Triple) {
  // Must match Z80TargetMachine data layout
  resetDataLayout("e-m:o-p:16:8-i16:8-i32:8-i64:8-i128:8-f32:8-f64:8-n8:16");

  PointerWidth = 16;
  PointerAlign = 8;
  ShortAlign = 8;
  IntWidth = 16;
  IntAlign = 8;
  LongWidth = 32;
  LongAlign = 8;
  LongLongWidth = 64;
  LongLongAlign = 8;
  Int128Align = 8;
  FloatAlign = 8;
  DoubleAlign = 8;
  LongDoubleAlign = 8;
  SuitableAlign = 8;
  DefaultAlignForAttributeAligned = 8;
  SizeType = UnsignedInt;
  PtrDiffType = SignedInt;
  IntPtrType = SignedInt;
  WCharType = UnsignedInt;
  WIntType = SignedInt;
  Char16Type = UnsignedInt;
  Char32Type = UnsignedLong;
  Int16Type = SignedInt;
  SigAtomicType = UnsignedChar;
}

bool Z80TargetInfo::validateAsmConstraint(
    const char *&Name, TargetInfo::ConstraintInfo &Info) const {
  switch (*Name) {
  default:
    return false;
  case 'a':
  case 'b':
  case 'c':
  case 'd':
  case 'e':
  case 'h':
  case 'l': // Individual 8-bit registers
  case 'r': // Any register (8/16-bit)
  case 'R': // 8-bit register class
    Info.setAllowsRegister();
    return true;
  }
}

static const char *const Z80GCCRegNames[] = {
    "a",  "b",  "c",  "d",  "e",  "h",  "l",  "f",
    "bc", "de", "hl", "af", "ix", "iy", "sp",
};

static const char *const SM83GCCRegNames[] = {
    "a", "b", "c", "d", "e", "h", "l", "f", "bc", "de", "hl", "af", "sp",
};

llvm::ArrayRef<const char *> Z80TargetInfo::getGCCRegNames() const {
  if (getTriple().isSM83())
    return SM83GCCRegNames;
  return Z80GCCRegNames;
}

Z80TargetInfo::CallingConvCheckResult
Z80TargetInfo::checkCallingConvention(CallingConv CC) const {
  switch (CC) {
  case CC_C:
  case CC_Z80SDCCCall0:
    return CCCR_OK;
  default:
    return CCCR_Warning;
  }
}

void Z80TargetInfo::getTargetDefines(const LangOptions &Opts,
                                     MacroBuilder &Builder) const {
  if (getTriple().isSM83()) {
    Builder.defineMacro("__sm83__");
    Builder.defineMacro("__SM83__");
    Builder.defineMacro("__GAMEBOY__");
  } else {
    Builder.defineMacro("__z80__");
    Builder.defineMacro("__Z80__");
  }
  // Z80/SM83 uses sdasz80 .rel object format, not ELF.
  // Do not define __ELF__.
}
