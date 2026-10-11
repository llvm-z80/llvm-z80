//===--- Z80CallingConv.cpp - Z80 calling convention attributes -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/Basic/Z80CallingConv.h"
#include "llvm/Support/ErrorHandling.h"

using namespace clang;

/// The base CallingConv::C has in \p TT's environment.
static Z80CCBase defaultZ80CCBase(const llvm::Triple &TT) {
  return TT.getEnvironment() == llvm::Triple::Z88DK ? Z80CCBase::SmallC
                                                     : Z80CCBase::SDCCCall1;
}

/// Split \p CC into its parts, or return nullopt for a convention outside the
/// Z80 family.  The modifier on the base of CallingConv::C comes back without
/// a base.
static std::optional<Z80CCParts> splitZ80CC(CallingConv CC,
                                            const llvm::Triple &TT) {
  Z80CCBase Default = defaultZ80CCBase(TT);
  Z80CCParts P;
  switch (CC) {
  case CC_C:
    P.Base = Default;
    break;
  case CC_Z80SDCCCall1:
    P.Base = Z80CCBase::SDCCCall1;
    break;
  case CC_Z80SDCCCall0:
    P.Base = Z80CCBase::SDCCCall0;
    break;
  case CC_Z80SmallC:
    P.Base = Z80CCBase::SmallC;
    break;
  case CC_Z80Z88dkFastCall:
    P.Base = Z80CCBase::Z88dkFastCall;
    break;
  case CC_Z80Z88dkCallee:
    P = {Z80CCBase::SDCCCall1, true};
    break;
  case CC_Z80SDCCCall0Callee:
    P = {Z80CCBase::SDCCCall0, true};
    break;
  case CC_Z80SmallCCallee:
    P = {Z80CCBase::SmallC, true};
    break;
  default:
    return std::nullopt;
  }
  if (P.Callee && P.Base == Default)
    P.Base = std::nullopt;
  return P;
}

/// Merge two different bases as SDCC does, or return nullopt if they
/// conflict.  __z88dk_fastcall overrides every other base, and __smallc
/// overrides __sdcccall(0) and __sdcccall(1).
static std::optional<Z80CCBase> mergeZ80CCBases(Z80CCBase A, Z80CCBase B) {
  if (A == Z80CCBase::Z88dkFastCall || B == Z80CCBase::Z88dkFastCall)
    return Z80CCBase::Z88dkFastCall;
  if (A == Z80CCBase::SmallC || B == Z80CCBase::SmallC)
    return Z80CCBase::SmallC;
  return std::nullopt;
}

CallingConv clang::joinZ80CC(Z80CCParts P, const llvm::Triple &TT) {
  Z80CCBase Default = defaultZ80CCBase(TT);
  Z80CCBase Base = P.Base.value_or(Default);
  if (!P.Callee && Base == Default)
    return CC_C;
  switch (Base) {
  case Z80CCBase::SDCCCall1:
    return P.Callee ? CC_Z80Z88dkCallee : CC_Z80SDCCCall1;
  case Z80CCBase::SDCCCall0:
    return P.Callee ? CC_Z80SDCCCall0Callee : CC_Z80SDCCCall0;
  case Z80CCBase::SmallC:
    return P.Callee ? CC_Z80SmallCCallee : CC_Z80SmallC;
  case Z80CCBase::Z88dkFastCall:
    // Nothing goes on the stack, so the modifier has nothing to change.
    return CC_Z80Z88dkFastCall;
  }
  llvm_unreachable("bad Z80 calling-convention base");
}

bool clang::composeZ80CallingConvs(CallingConv A, CallingConv B,
                                   const llvm::Triple &TT,
                                   CallingConv &Composed) {
  std::optional<Z80CCParts> PA = splitZ80CC(A, TT);
  std::optional<Z80CCParts> PB = splitZ80CC(B, TT);
  if (!PA || !PB)
    return false;
  Z80CCParts P = {PA->Base ? PA->Base : PB->Base, PA->Callee || PB->Callee};
  if (PA->Base && PB->Base && *PA->Base != *PB->Base) {
    P.Base = mergeZ80CCBases(*PA->Base, *PB->Base);
    if (!P.Base)
      return false;
  }
  Composed = joinZ80CC(P, TT);
  return true;
}
