//===--------- Z80MCELFStreamer.cpp - Z80 subclass of MCELFStreamer -------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/Z80MCELFStreamer.h"
#include "Z80MCTargetDesc.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFStreamer.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCObjectFileInfo.h"

using namespace llvm;

namespace llvm {

void Z80MCELFStreamer::initSections(const MCSubtargetInfo &STI) {
  MCContext &Ctx = getContext();
  switchSection(Ctx.getObjectFileInfo()->getTextSection());
  emitCodeAlignment(Align(1), STI);
}

static bool HasPrefix(StringRef Name, StringRef Prefix) {
  SmallString<32> PrefixDot = Prefix;
  PrefixDot += ".";
  return Name == Prefix || Name.starts_with(PrefixDot);
}

void Z80MCELFStreamer::changeSection(MCSection *Section, uint32_t Subsection) {
  MCELFStreamer::changeSection(Section, Subsection);
  HasBSS |= HasPrefix(Section->getName(), ".bss");
  HasData |= HasPrefix(Section->getName(), ".data");
  HasInitArray |= HasPrefix(Section->getName(), ".init_array");
  HasFiniArray |= HasPrefix(Section->getName(), ".fini_array");
}

void Z80MCELFStreamer::emitInstruction(const MCInst &Inst,
                                       const MCSubtargetInfo &STI) {
  MCELFStreamer::emitInstruction(Inst, STI);
}

MCStreamer *createZ80MCELFStreamer(const Triple & /*T*/, MCContext &Ctx,
                                   std::unique_ptr<MCAsmBackend> &&TAB,
                                   std::unique_ptr<MCObjectWriter> &&OW,
                                   std::unique_ptr<MCCodeEmitter> &&Emitter) {
  auto *S = new Z80MCELFStreamer(Ctx, std::move(TAB), std::move(OW),
                                 std::move(Emitter));
  return S;
}

} // end namespace llvm
