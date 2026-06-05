//===- Z80.cpp - Z80/SM83 target CodeGen info -----------------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ABIInfoImpl.h"
#include "TargetInfo.h"

using namespace clang;
using namespace clang::CodeGen;

namespace {
class Z80TargetCodeGenInfo : public TargetCodeGenInfo {
public:
  Z80TargetCodeGenInfo(CodeGenTypes &CGT)
      : TargetCodeGenInfo(std::make_unique<DefaultABIInfo>(CGT)) {}

  void setTargetAttributes(const Decl *D, llvm::GlobalValue *GV,
                           CodeGen::CodeGenModule &CGM) const override {
    if (GV->isDeclaration())
      return;
    const auto *FD = dyn_cast_or_null<FunctionDecl>(D);
    if (!FD)
      return;
    auto *Fn = cast<llvm::Function>(GV);

    if (FD->getAttr<Z80InterruptAttr>())
      Fn->addFnAttr("interrupt");
  }
};
} // namespace

std::unique_ptr<TargetCodeGenInfo>
CodeGen::createZ80TargetCodeGenInfo(CodeGenModule &CGM) {
  return std::make_unique<Z80TargetCodeGenInfo>(CGM.getTypes());
}
