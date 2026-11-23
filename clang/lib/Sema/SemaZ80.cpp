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

} // namespace clang
