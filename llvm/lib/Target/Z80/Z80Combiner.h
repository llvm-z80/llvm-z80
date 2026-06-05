//===-- Z80Combiner.h - Z80 GlobalIsel Combiner -----------------*- C++ -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_Z80_Z80COMBINER_H
#define LLVM_LIB_TARGET_Z80_Z80COMBINER_H

#include "llvm/Pass.h"

namespace llvm {

FunctionPass *createZ80PreLegalizerCombiner();
FunctionPass *createZ80PostLegalizerCombiner();

} // end namespace llvm

#endif // not LLVM_LIB_TARGET_Z80_Z80COMBINER_H
