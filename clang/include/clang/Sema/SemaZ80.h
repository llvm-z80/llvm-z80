//===----------------------------------------------------------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file declares semantic analysis functions specific to Z80 targets.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_SEMA_SEMAZ80_H
#define LLVM_CLANG_SEMA_SEMAZ80_H

#include "clang/AST/ASTFwd.h"
#include "clang/Sema/SemaBase.h"

namespace clang {
class ParsedAttr;
class Z80PreservesRegsAttr;

class SemaZ80 : public SemaBase {
public:
  SemaZ80(Sema &S);

  void handleInterruptAttr(Decl *D, const ParsedAttr &AL);
  void handlePreservesRegsAttr(Decl *D, const ParsedAttr &AL);
  Z80PreservesRegsAttr *mergePreservesRegsAttr(Decl *D,
                                               const Z80PreservesRegsAttr &AL);
};

} // namespace clang

#endif // LLVM_CLANG_SEMA_SEMAZ80_H
