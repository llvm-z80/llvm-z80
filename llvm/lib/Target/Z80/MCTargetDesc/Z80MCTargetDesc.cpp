//===-- Z80MCTargetDesc.cpp - Z80 Target Descriptions ---------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides Z80 specific target descriptions.
//
//===----------------------------------------------------------------------===//

#include "Z80MCTargetDesc.h"
#include "TargetInfo/Z80TargetInfo.h"
#include "Z80InstPrinter.h"
#include "Z80MCAsmInfo.h"
#include "Z80MCELFStreamer.h"
#include "Z80MCInstrAnalysis.h"
#include "Z80TargetStreamer.h"

#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCELFStreamer.h"
#include "llvm/MC/MCInstrAnalysis.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"

#define GET_INSTRINFO_MC_DESC
#include "Z80GenInstrInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "Z80GenSubtargetInfo.inc"

#define GET_REGINFO_MC_DESC
#include "Z80GenRegisterInfo.inc"

using namespace llvm;

MCInstrInfo *llvm::createZ80MCInstrInfo() {
  MCInstrInfo *X = new MCInstrInfo();
  InitZ80MCInstrInfo(X);

  return X;
}

static MCRegisterInfo *createZ80MCRegisterInfo(const Triple &TT) {
  MCRegisterInfo *X = new MCRegisterInfo();
  InitZ80MCRegisterInfo(X, 0);

  return X;
}

static MCSubtargetInfo *createZ80MCSubtargetInfo(const Triple &TT,
                                                 StringRef CPU, StringRef FS) {
  if (CPU.empty())
    CPU = (TT.getArch() == Triple::sm83) ? "sm83" : "z80";
  return createZ80MCSubtargetInfoImpl(TT, CPU, /*TuneCPU*/ CPU, FS);
}

static MCAsmInfo *createZ80MCAsmInfo(const MCRegisterInfo &MRI,
                                     const Triple &TT,
                                     const MCTargetOptions &Options) {
  bool UseSDASZ80 = TT.getEnvironment() == Triple::SDCC;
  if (Z80AsmFormat.getNumOccurrences())
    UseSDASZ80 = Z80AsmFormat == Z80AsmFormat_SDASZ80;

  if (UseSDASZ80)
    return new Z80MCAsmInfoSDCC(TT, Options);
  return new Z80MCAsmInfo(TT, Options);
}

static MCInstPrinter *createZ80MCInstPrinter(const Triple &T,
                                             unsigned SyntaxVariant,
                                             const MCAsmInfo &MAI,
                                             const MCInstrInfo &MII,
                                             const MCRegisterInfo &MRI) {
  if (SyntaxVariant == 0)
    return new Z80InstPrinter(MAI, MII, MRI);
  if (SyntaxVariant == 1)
    return new Z80InstPrinterSDCC(MAI, MII, MRI);
  return nullptr;
}

static MCTargetStreamer *
createZ80ObjectTargetStreamer(MCStreamer &S, const MCSubtargetInfo &STI) {
  return new Z80TargetELFStreamer(S, STI);
}

static MCTargetStreamer *createMCAsmTargetStreamer(MCStreamer &S,
                                                   formatted_raw_ostream &OS,
                                                   MCInstPrinter *InstPrint) {
  return new Z80TargetAsmStreamer(S, OS);
}

static MCInstrAnalysis *createZ80MCInstrAnalysis(const MCInstrInfo *Info) {
  return new Z80MCInstrAnalysis(Info);
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeZ80TargetMC() {
  // Register MC components for both Z80 and SM83 targets.
  for (Target *T : {&getTheZ80Target(), &getTheSM83Target()}) {
    RegisterMCAsmInfoFn X(*T, createZ80MCAsmInfo);
    TargetRegistry::RegisterMCInstrInfo(*T, createZ80MCInstrInfo);
    TargetRegistry::RegisterMCRegInfo(*T, createZ80MCRegisterInfo);
    TargetRegistry::RegisterMCSubtargetInfo(*T, createZ80MCSubtargetInfo);
    TargetRegistry::RegisterMCInstPrinter(*T, createZ80MCInstPrinter);
    TargetRegistry::RegisterMCInstrAnalysis(*T, createZ80MCInstrAnalysis);
    TargetRegistry::RegisterMCCodeEmitter(*T, createZ80MCCodeEmitter);
    TargetRegistry::RegisterELFStreamer(*T, createZ80MCELFStreamer);
    TargetRegistry::RegisterObjectTargetStreamer(*T,
                                                 createZ80ObjectTargetStreamer);
    TargetRegistry::RegisterAsmTargetStreamer(*T, createMCAsmTargetStreamer);
    TargetRegistry::RegisterMCAsmBackend(*T, createZ80AsmBackend);
  }
}
