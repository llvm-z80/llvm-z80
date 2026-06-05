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

#include "llvm/BinaryFormat/ELF.h"
#include "llvm/CodeGen/GlobalISel/CallLowering.h"
#include "llvm/CodeGen/GlobalISel/InstructionSelector.h"
#include "llvm/CodeGen/GlobalISel/Utils.h"
#include "llvm/MC/TargetRegistry.h"

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "SM83CallLowering.h"
#include "Z80.h"
#include "Z80CallLowering.h"
#include "Z80FrameLowering.h"
#include "Z80InstructionSelector.h"
#include "Z80LegalizerInfo.h"
#include "Z80TargetMachine.h"

#define DEBUG_TYPE "z80-subtarget"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "Z80GenSubtargetInfo.inc"

using namespace llvm;

Z80Subtarget::Z80Subtarget(const Triple &TT, const std::string &CPU,
                           const std::string &FS, const Z80TargetMachine &TM)
    : Z80GenSubtargetInfo(TT, CPU, /* TuneCPU */ CPU, FS), InstrInfo(*this),
      RegInfo(), FrameLowering(),
      TLInfo(TM, initializeSubtargetDependencies(CPU, FS, TM)),
      Legalizer(*this),
      InstSelector(createZ80InstructionSelector(TM, *this, RegBankInfo)),
      InlineAsmLoweringInfo(&TLInfo) {
  // Create the appropriate CallLowering after features are parsed.
  // initializeSubtargetDependencies (called during TLInfo init) sets HasSM83.
  if (hasSM83())
    CallLoweringInfo = std::make_unique<SM83CallLowering>(&TLInfo);
  else
    CallLoweringInfo = std::make_unique<Z80CallLowering>(&TLInfo);
}

Z80Subtarget &
Z80Subtarget::initializeSubtargetDependencies(StringRef CPU, StringRef FS,
                                              const TargetMachine &TM) {
  // Parse features string.
  ParseSubtargetFeatures(CPU, /* TuneCPU */ CPU, FS);

  return *this;
}
