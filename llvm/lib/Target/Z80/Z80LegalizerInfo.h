//===-- Z80LegalizerInfo.h - Z80 Legalizer ----------------------*- C++ -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the interface that Z80 uses to legalize generic MIR.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_Z80_Z80LEGALIZERINFO_H
#define LLVM_LIB_TARGET_Z80_Z80LEGALIZERINFO_H

#include "llvm/CodeGen/GlobalISel/LegalizerInfo.h"

namespace llvm {

class Z80Subtarget;

class Z80LegalizerInfo : public LegalizerInfo {
public:
  Z80LegalizerInfo(const Z80Subtarget &STI);

  bool legalizeIntrinsic(LegalizerHelper &Helper,
                         MachineInstr &MI) const override;

  bool legalizeCustom(LegalizerHelper &Helper, MachineInstr &MI,
                      LostDebugLocObserver &LocObserver) const override;
};

} // namespace llvm

#endif // not LLVM_LIB_TARGET_Z80_Z80LEGALIZERINFO_H
