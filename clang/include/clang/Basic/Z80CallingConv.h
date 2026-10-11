//===--- Z80CallingConv.h - Z80 calling convention attributes ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_BASIC_Z80CALLINGCONV_H
#define LLVM_CLANG_BASIC_Z80CALLINGCONV_H

#include "clang/Basic/Specifiers.h"
#include "llvm/TargetParser/Triple.h"
#include <optional>

namespace clang {

/// The argument-passing base of a Z80 calling convention.
enum class Z80CCBase { SDCCCall1, SDCCCall0, SmallC, Z88dkFastCall };

/// A Z80 calling convention as its base and the __z88dk_callee modifier.  A
/// convention without a base is the modifier alone.
struct Z80CCParts {
  std::optional<Z80CCBase> Base;
  bool Callee = false;
};

/// Join \p P into a convention for \p TT.  \p P without a base gets the base
/// of CallingConv::C.
CallingConv joinZ80CC(Z80CCParts P, const llvm::Triple &TT);

/// Compose two Z80 calling conventions written on one function as SDCC does.
/// Returns false if they conflict.  Attributes are composed two at a time, so
/// __sdcccall(0) __smallc __sdcccall(1) is accepted as __smallc.
bool composeZ80CallingConvs(CallingConv A, CallingConv B,
                            const llvm::Triple &TT, CallingConv &Composed);

} // namespace clang

#endif // LLVM_CLANG_BASIC_Z80CALLINGCONV_H
