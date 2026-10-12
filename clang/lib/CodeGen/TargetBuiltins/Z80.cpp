//===-- Z80.cpp - Emit LLVM Code for Z80 builtins -------------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file emits Z80 builtin calls as the matching llvm.z80.* intrinsics.
// ravn/llvm-z80#42.
//
//===----------------------------------------------------------------------===//

#include "CodeGenFunction.h"
#include "clang/Basic/TargetBuiltins.h"
#include "llvm/IR/IntrinsicsZ80.h"

using namespace clang;
using namespace CodeGen;
using namespace llvm;

Value *CodeGenFunction::EmitZ80BuiltinExpr(unsigned BuiltinID,
                                           const CallExpr *E) {
  // Each privileged builtin lowers to its side-effecting llvm.z80.* intrinsic.
  Intrinsic::ID IntID;
  switch (BuiltinID) {
  default:
    return nullptr;
  case clang::Z80::BI__builtin_z80_di:
    IntID = Intrinsic::z80_di;
    break;
  case clang::Z80::BI__builtin_z80_ei:
    IntID = Intrinsic::z80_ei;
    break;
  case clang::Z80::BI__builtin_z80_halt:
    IntID = Intrinsic::z80_halt;
    break;
  case clang::Z80::BI__builtin_z80_nop:
    IntID = Intrinsic::z80_nop;
    break;
  case clang::Z80::BI__builtin_z80_im2:
    IntID = Intrinsic::z80_im2;
    break;
  case clang::Z80::BI__builtin_z80_set_i: {
    // void __builtin_z80_set_i(unsigned char) -> llvm.z80.set_i(i8)
    Value *Val = EmitScalarExpr(E->getArg(0));
    Function *F =
        Intrinsic::getOrInsertDeclaration(&CGM.getModule(), Intrinsic::z80_set_i);
    return Builder.CreateCall(F, {Val});
  }
  }

  Function *F = Intrinsic::getOrInsertDeclaration(&CGM.getModule(), IntID);
  return Builder.CreateCall(F);
}
