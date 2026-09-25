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
#include "clang/Basic/TargetBuiltins.h"
#include "clang/Basic/TargetInfo.h"
#include "llvm/ADT/STLExtras.h"

using namespace clang;
using namespace clang::targets;

static constexpr int NumBuiltins =
    clang::Z80::LastTSBuiltin - Builtin::FirstTSBuiltin;

#define GET_BUILTIN_STR_TABLE
#include "clang/Basic/BuiltinsZ80.inc"
#undef GET_BUILTIN_STR_TABLE

static constexpr Builtin::Info BuiltinInfos[] = {
#define GET_BUILTIN_INFOS
#include "clang/Basic/BuiltinsZ80.inc"
#undef GET_BUILTIN_INFOS
};
static_assert(std::size(BuiltinInfos) == NumBuiltins);

SmallVector<Builtin::InfosShard> Z80TargetInfo::getTargetBuiltins() const {
  return {{&BuiltinStrings, BuiltinInfos}};
}

Z80TargetInfo::Z80TargetInfo(const llvm::Triple &Triple, const TargetOptions &)
    : TargetInfo(Triple) {
  // Must match Z80TargetMachine data layout
  resetDataLayout("e-m:o-p:16:8-i16:8-i32:8-i64:8-i128:8-f32:8-f64:8-ve-n8:16");

  // The data layout mangles globals with a leading underscore (sdas
  // convention); the frontend prefix must agree, and a non-empty prefix is
  // also what makes asm("name") renames emit their exact spelling.
  UserLabelPrefix = "_";

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
  // The fixed-point types (_Accum/_Fract) and the storage-only float types
  // (__fp16, __bf16) have their own layout fields and default to their
  // natural alignment; everything is byte-aligned here.
  ShortAccumAlign = 8;
  AccumAlign = 8;
  LongAccumAlign = 8;
  ShortFractAlign = 8;
  FractAlign = 8;
  LongFractAlign = 8;
  HalfAlign = 8;
  BFloat16Align = 8;
  // Vectors take their element's byte alignment (the "ve" datalayout token);
  // their natural alignment cannot be honored on a byte-aligned stack.
  VectorsAreElementAligned = true;
  MaxVectorAlign = 8;
  SuitableAlign = 8;
  DefaultAlignForAttributeAligned = 8;
  MaxAtomicPromoteWidth = MaxAtomicInlineWidth = 8;
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
  case CC_Z80SmallC:
  case CC_Z80Z88dkCallee:
  case CC_Z80SDCCCall0Callee:
  case CC_Z80SmallCCallee:
    return CCCR_OK;
  case CC_Z80Z88dkFastCall:
    return getTriple().isSM83() ? CCCR_Warning : CCCR_OK;
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
  // compiler-rt/{z80,sm83} has no complex helpers, so `a * b` and `a / b` on
  // _Complex would only fail at link time with an undefined __mulsc3 or
  // __divsc3. Say so up front instead; portable code guards <complex.h> on
  // this macro.
  Builder.defineMacro("__STDC_NO_COMPLEX__");

  // Z80/SM83 uses sdasz80 .rel object format, not ELF.
  // Do not define __ELF__.
}

bool Z80TargetInfo::isValidFeatureName(StringRef Feature) const {
  // The subtarget features Z80Features.td declares, which must be listed here
  // to be spelled in a target attribute: without this the base class accepts
  // every name, a misspelling reaches the backend as a feature it does not
  // know, and the attribute quietly does nothing. Keep in step with that file.
  static constexpr StringRef Known[] = {
      "z80",          "z180",         "r800",
      "ez80",         "sm83",         "undocumented",
      "static-frame", "inline-i16-runtime",
  };
  return llvm::is_contained(Known, Feature);
}

bool Z80TargetInfo::initFeatureMap(
    llvm::StringMap<bool> &Features, DiagnosticsEngine &Diags, StringRef CPU,
    const std::vector<std::string> &FeaturesVec) const {
  bool Ok = TargetInfo::initFeatureMap(Features, Diags, CPU, FeaturesVec);
  // The base Z80 ISA is implied for the z80 triple but not for sm83 (Game Boy),
  // which has no interrupt modes and no I register. This gates the z80-only
  // builtins __builtin_z80_im2 / __builtin_z80_set_i (Features = "z80" in
  // BuiltinsZ80.td) so Sema rejects them on sm83 with a clean diagnostic
  // instead of a backend cannot-select.
  if (!getTriple().isSM83())
    Features["z80"] = true;
  return Ok;
}

bool Z80TargetInfo::hasFeature(StringRef Feature) const {
  return Feature == "z80" ? !getTriple().isSM83() : false;
}
