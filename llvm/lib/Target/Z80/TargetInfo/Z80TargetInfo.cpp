//===-- Z80TargetInfo.cpp - Z80 Target Implementation ---------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TargetInfo/Z80TargetInfo.h"
#include "llvm/MC/TargetRegistry.h"

namespace llvm {
Target &getTheZ80Target() {
  static Target TheZ80Target;
  return TheZ80Target;
}
Target &getTheSM83Target() {
  static Target TheSM83Target;
  return TheSM83Target;
}
} // namespace llvm

extern "C" LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeZ80TargetInfo() { // NOLINT
  llvm::RegisterTarget<llvm::Triple::z80> X(llvm::getTheZ80Target(), "z80",
                                            "Zilog Z80 and variants", "Z80");
  llvm::RegisterTarget<llvm::Triple::sm83> Y(llvm::getTheSM83Target(), "sm83",
                                             "Sharp SM83 (Game Boy)", "Z80");
}
