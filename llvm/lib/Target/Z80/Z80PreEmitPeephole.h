//===-- Z80PreEmitPeephole.h - Z80 pre-emit peephole ------------*- C++ -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the Z80 pre-emit peephole optimization pass.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_Z80_Z80PREEMITPEEPHOLE_H
#define LLVM_LIB_TARGET_Z80_Z80PREEMITPEEPHOLE_H

#include "llvm/CodeGen/MachineFunctionPass.h"

namespace llvm {

MachineFunctionPass *createZ80PreEmitPeepholePass();

} // namespace llvm

#endif // not LLVM_LIB_TARGET_Z80_Z80PREEMITPEEPHOLE_H
