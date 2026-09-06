//===-- Z80StaticFrameAlloc.h - Z80 Static Frame Allocation -----*- C++ -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Lays the static frames of non-reentrant functions out in one global,
// overlaying the frames of functions that can never be active at the same
// time, and resolves the placeholder operands to the resulting symbols.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_Z80_Z80STATICFRAMEALLOC_H
#define LLVM_LIB_TARGET_Z80_Z80STATICFRAMEALLOC_H

namespace llvm {

class ModulePass;
class PassRegistry;

ModulePass *createZ80StaticFrameAllocPass();
void initializeZ80StaticFrameAllocPass(PassRegistry &);

} // namespace llvm

#endif // LLVM_LIB_TARGET_Z80_Z80STATICFRAMEALLOC_H
