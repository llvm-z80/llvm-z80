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
class Z80ABIInfo : public DefaultABIInfo {
public:
  Z80ABIInfo(CodeGenTypes &CGT) : DefaultABIInfo(CGT) {}

  RValue EmitVAArg(CodeGenFunction &CGF, Address VAListAddr, QualType Ty,
                   AggValueSlot Slot) const override {
    // A byval aggregate's bytes are copied into the argument area by the
    // caller, so va_arg must read them in place; the default emission
    // would read a pointer that was never stored. Arguments are packed on
    // the stack, so the slot unit is one byte. Everything else (scalars,
    // and C++ records that really do pass a pointer) keeps the default
    // path, mirroring the caller's classification.
    ABIArgInfo AI = classifyArgumentType(Ty);
    if (AI.isIndirect() && AI.getIndirectByVal())
      return emitVoidPtrVAArg(CGF, VAListAddr, Ty, /*IsIndirect=*/false,
                              getContext().getTypeInfoInChars(Ty),
                              CharUnits::One(), /*AllowHigherAlign=*/false,
                              Slot);
    return DefaultABIInfo::EmitVAArg(CGF, VAListAddr, Ty, Slot);
  }
};

class Z80TargetCodeGenInfo : public TargetCodeGenInfo {
public:
  Z80TargetCodeGenInfo(CodeGenTypes &CGT)
      : TargetCodeGenInfo(std::make_unique<Z80ABIInfo>(CGT)) {}

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
