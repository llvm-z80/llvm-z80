//===-- Z80NonReentrant.h - Z80 NonReentrant Pass ---------------*- C++ -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass examines the whole-module call graph to find functions that can
// have at most one activation live at any time. Their stack frames can then
// be laid out in static memory.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_Z80_Z80NONREENTRANT_H
#define LLVM_LIB_TARGET_Z80_Z80NONREENTRANT_H

namespace llvm {

class ModulePass;
class PassRegistry;

ModulePass *createZ80NonReentrantPass();
void initializeZ80NonReentrantPass(PassRegistry &);

} // namespace llvm

#endif // LLVM_LIB_TARGET_Z80_Z80NONREENTRANT_H
