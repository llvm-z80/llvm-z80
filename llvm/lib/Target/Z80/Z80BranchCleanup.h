//===-- Z80BranchCleanup.h - Z80 Branch Cleanup -----------------*- C++ -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_Z80_Z80BRANCHCLEANUP_H
#define LLVM_LIB_TARGET_Z80_Z80BRANCHCLEANUP_H

#include "llvm/CodeGen/MachineFunctionPass.h"

namespace llvm {

MachineFunctionPass *createZ80BranchCleanupPass();

} // namespace llvm

#endif // LLVM_LIB_TARGET_Z80_Z80BRANCHCLEANUP_H
