//===-- Z80LowerThreadLocal.h - Z80 thread-local variable lowering --------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_Z80_Z80LOWERTHREADLOCAL_H
#define LLVM_LIB_TARGET_Z80_Z80LOWERTHREADLOCAL_H

namespace llvm {

class ModulePass;
class PassRegistry;
class TargetMachine;

ModulePass *createZ80LowerThreadLocalPass(const TargetMachine &TM);
void initializeZ80LowerThreadLocalPass(PassRegistry &);

} // namespace llvm

#endif // LLVM_LIB_TARGET_Z80_Z80LOWERTHREADLOCAL_H
