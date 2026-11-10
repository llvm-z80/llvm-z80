//===-- Z80PushPopSpills.h - Spill with PUSH and POP ------------*- C++ -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Replaces 16-bit spill slots used in stack order within one basic block with
// SPILL_PUSH and SPILL_POP.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_Z80_Z80PUSHPOPSPILLS_H
#define LLVM_LIB_TARGET_Z80_Z80PUSHPOPSPILLS_H

namespace llvm {

class MachineFunctionPass;
class PassRegistry;

MachineFunctionPass *createZ80PushPopSpillsPass();
void initializeZ80PushPopSpillsPass(PassRegistry &);

} // namespace llvm

#endif // LLVM_LIB_TARGET_Z80_Z80PUSHPOPSPILLS_H
