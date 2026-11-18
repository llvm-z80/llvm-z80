//===-- Z80Subtarget.cpp - Z80 Subtarget Information ----------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the Z80 specific subclass of TargetSubtargetInfo.
//
//===----------------------------------------------------------------------===//

#include "Z80Subtarget.h"
#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "SM83CallLowering.h"
#include "Z80CallLowering.h"
#include "Z80InstructionSelector.h"

#define DEBUG_TYPE "z80-subtarget"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "Z80GenSubtargetInfo.inc"

using namespace llvm;

Z80Subtarget::Z80Subtarget(const Triple &TT, StringRef CPU, StringRef FS,
                           const Z80TargetMachine &TM)
    : Z80GenSubtargetInfo(TT, CPU, /* TuneCPU */ CPU, FS),
      InstrInfo(initializeSubtargetDependencies(CPU, FS)), TLInfo(TM, *this),
      Legalizer(*this),
      InstSelector(createZ80InstructionSelector(TM, *this, RegBankInfo)),
      InlineAsmLoweringInfo(&TLInfo) {
  if (hasSM83())
    CallLoweringInfo = std::make_unique<SM83CallLowering>(&TLInfo);
  else
    CallLoweringInfo = std::make_unique<Z80CallLowering>(&TLInfo);
}

Z80Subtarget &Z80Subtarget::initializeSubtargetDependencies(StringRef CPU,
                                                            StringRef FS) {
  ParseSubtargetFeatures(CPU, /* TuneCPU */ CPU, FS);
  return *this;
}
