//===-- Z80ExpandPseudo.h - Z80 Pseudo Expansion Pass -----------*- C++ -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the Z80 pseudo instruction expansion pass.
// Handles pseudos that require MBB splitting (e.g., variable shift loops).
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_Z80_Z80EXPANDPSEUDO_H
#define LLVM_LIB_TARGET_Z80_Z80EXPANDPSEUDO_H

#include "llvm/CodeGen/MachineFunctionPass.h"

namespace llvm {

MachineFunctionPass *createZ80ExpandPseudoPass();

} // namespace llvm

#endif // LLVM_LIB_TARGET_Z80_Z80EXPANDPSEUDO_H
