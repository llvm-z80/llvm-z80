//===-- Z80.h - Top-level interface for Z80 representation ------*- C++ -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the entry points for global functions defined in the LLVM
// Z80 back-end.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_Z80_Z80_H
#define LLVM_LIB_TARGET_Z80_Z80_H

#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/Pass.h"

namespace llvm {

void initializeZ80BranchCleanupPass(PassRegistry &);
void initializeZ80PreLegalizerCombinerPass(PassRegistry &);
void initializeZ80PostLegalizerCombinerPass(PassRegistry &);
void initializeZ80ExpandPseudoPass(PassRegistry &);
void initializeZ80FixupImplicitDefsPass(PassRegistry &);
void initializeZ80IndexIVPass(PassRegistry &);
void initializeZ80LateOptimizationPass(PassRegistry &);
void initializeZ80LowerSelectPass(PassRegistry &);
void initializeZ80PostRAScavengingPass(PassRegistry &);
void initializeZ80ShiftRotateChainPass(PassRegistry &);

// The behind-by-one property of the std::reverse_iterator adaptor applied by
// reverse() does not properly handle instruction erasures. This range construct
// converts the forward iterators to native reverse iterators that are not
// behind-by-one and therefore handle erasures correctly when combined with
// make_early_inc_range().
inline auto mbb_reverse(MachineBasicBlock::iterator Begin,
                        MachineBasicBlock::iterator End) {
  return make_range(MachineBasicBlock::reverse_iterator(End),
                    MachineBasicBlock::reverse_iterator(Begin));
}
template <typename ContainerTy> inline auto mbb_reverse(ContainerTy &&C) {
  return mbb_reverse(C.begin(), C.end());
}

} // namespace llvm

#endif // not LLVM_LIB_TARGET_Z80_Z80_H
