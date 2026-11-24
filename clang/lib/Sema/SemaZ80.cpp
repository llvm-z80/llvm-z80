//===----------------------------------------------------------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file implements semantic analysis functions specific to Z80 targets.
///
//===----------------------------------------------------------------------===//

#include "clang/Sema/SemaZ80.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclBase.h"
#include "clang/Basic/DiagnosticSema.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Sema/Attr.h"
#include "clang/Sema/ParsedAttr.h"
#include "clang/Sema/Sema.h"
#include "llvm/ADT/SmallPtrSet.h"

namespace clang {
SemaZ80::SemaZ80(Sema &S) : SemaBase(S) {}

void SemaZ80::handleInterruptAttr(Decl *D, const ParsedAttr &AL) {
  if (!isFuncOrMethodForAttrSubject(D)) {
    Diag(D->getLocation(), diag::warn_attribute_wrong_decl_type)
        << AL << AL.isRegularKeywordAttribute() << ExpectedFunction;
    return;
  }

  if (hasFunctionProto(D) && getFunctionOrMethodNumParams(D) != 0) {
    Diag(D->getLocation(), diag::warn_interrupt_signal_attribute_invalid)
        << /*Z80*/ 4 << /*interrupt*/ 0 << 0;
    return;
  }
  if (!getFunctionOrMethodResultType(D)->isVoidType()) {
    Diag(D->getLocation(), diag::warn_interrupt_signal_attribute_invalid)
        << /*Z80*/ 4 << /*interrupt*/ 0 << 1;
    return;
  }

  if (!AL.checkAtMostNumArgs(SemaRef, 1))
    return;

  StringRef Str;
  SourceLocation ArgLoc;
  if (AL.getNumArgs() == 1 &&
      !SemaRef.checkStringLiteralArgumentAttr(AL, 0, Str, &ArgLoc))
    return;

  Z80InterruptAttr::InterruptType Kind;
  if (!Z80InterruptAttr::ConvertStrToInterruptType(Str, Kind)) {
    Diag(AL.getLoc(), diag::warn_attribute_type_not_supported)
        << AL << Str << ArgLoc;
    return;
  }

  // SM83 has no non-maskable interrupt.
  if (Kind == Z80InterruptAttr::NMI &&
      getASTContext().getTargetInfo().getTriple().getArch() ==
          llvm::Triple::sm83) {
    Diag(AL.getLoc(), diag::err_z80_interrupt_nmi_sm83);
    return;
  }

  D->addAttr(::new (getASTContext())
                 Z80InterruptAttr(getASTContext(), AL, Kind));
}

void SemaZ80::handlePreservesRegsAttr(Decl *D, const ParsedAttr &AL) {
  if (!AL.checkAtLeastNumArgs(SemaRef, 1))
    return;

  static constexpr StringRef Z80Regs[] = {
      "a", "b", "c", "d", "e", "h", "l", "iyl", "iyh",
  };
  static constexpr StringRef SM83Regs[] = {
      "a", "b", "c", "d", "e", "h", "l",
  };
  ArrayRef<StringRef> KnownRegs = Z80Regs;
  if (getASTContext().getTargetInfo().getTriple().isSM83())
    KnownRegs = SM83Regs;
  SmallVector<const IdentifierInfo *, 9> Regs;
  for (unsigned I = 0, E = AL.getNumArgs(); I != E; ++I) {
    if (!AL.isArgIdent(I)) {
      Diag(AL.getLoc(), diag::err_attribute_argument_n_type)
          << AL << I + 1 << AANT_ArgumentIdentifier;
      return;
    }
    IdentifierLoc *Arg = AL.getArgAsIdent(I);
    const IdentifierInfo *Reg = Arg->getIdentifierInfo();
    if (!llvm::is_contained(KnownRegs, Reg->getName())) {
      Diag(Arg->getLoc(), diag::warn_attribute_type_not_supported)
          << AL << Reg->getName();
      continue;
    }
    Regs.push_back(Reg);
  }
  if (Regs.empty())
    return;

  D->addAttr(::new (getASTContext()) Z80PreservesRegsAttr(
      getASTContext(), AL, Regs.data(), Regs.size()));
}

Z80PreservesRegsAttr *
SemaZ80::mergePreservesRegsAttr(Decl *D, const Z80PreservesRegsAttr &AL) {
  const auto *Existing = D->getAttr<Z80PreservesRegsAttr>();
  if (!Existing)
    return AL.clone(getASTContext());
  // Every declaration must list the same registers.
  if (llvm::SmallPtrSet<const IdentifierInfo *, 9>(Existing->regs_begin(),
                                                   Existing->regs_end()) !=
      llvm::SmallPtrSet<const IdentifierInfo *, 9>(AL.regs_begin(),
                                                   AL.regs_end())) {
    Diag(Existing->getLocation(), diag::err_duplicate_attribute) << *Existing;
    Diag(AL.getLoc(), diag::note_previous_attribute);
  }
  return nullptr;
}

} // namespace clang
