//===-- Z80AccumulatorCopies.h - Isolate accumulator operands ---*- C++ -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the Z80 accumulator copy insertion pass.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_Z80_Z80ACCUMULATORCOPIES_H
#define LLVM_LIB_TARGET_Z80_Z80ACCUMULATORCOPIES_H

#include "llvm/CodeGen/MachineFunctionPass.h"

namespace llvm {

MachineFunctionPass *createZ80AccumulatorCopiesPass();

} // namespace llvm

#endif // not LLVM_LIB_TARGET_Z80_Z80ACCUMULATORCOPIES_H
