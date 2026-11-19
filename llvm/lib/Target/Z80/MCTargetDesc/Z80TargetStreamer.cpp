//===-- Z80TargetStreamer.cpp - Z80 Target Streamer Methods ---------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides Z80 specific target streamer methods.
//
//===----------------------------------------------------------------------===//

#include "Z80TargetStreamer.h"

#include "Z80MCAsmInfo.h"
#include "Z80MCELFStreamer.h"

#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Format.h"

namespace llvm {

Z80TargetStreamer::Z80TargetStreamer(MCStreamer &S) : MCTargetStreamer(S) {}

void Z80TargetAsmStreamer::changeSection(const MCSection *CurSection,
                                         MCSection *Section,
                                         uint32_t SubSection, raw_ostream &OS) {
  // Without a z88dk name or an sdasz80 area the contents would land in the
  // previous section.
  MCContext &Ctx = getStreamer().getContext();
  if (Ctx.getAsmInfo().isZ88DK() &&
      Z80MCAsmInfoZ88DK::getSectionName(Section->getName()).empty())
    Ctx.reportError(SMLoc(), "section '" + Section->getName() +
                                 "' cannot be emitted in the z88dk format");
  if (Ctx.getAsmInfo().isSDCC() &&
      Z80MCAsmInfoSDCC::getAreaName(Section->getName()).empty())
    Ctx.reportError(SMLoc(), "section '" + Section->getName() +
                                 "' cannot be emitted in the sdasz80 format; "
                                 "start the name with '_' to use it as an "
                                 "area name");
  MCTargetStreamer::changeSection(CurSection, Section, SubSection, OS);
}

void Z80TargetAsmStreamer::emitRawBytes(StringRef Data) {
  if (!getStreamer().getContext().getAsmInfo().isZ88DK())
    return MCTargetStreamer::emitRawBytes(Data);

  // Short lines keep the output within the line buffers of z88dk tools.
  constexpr size_t MaxChunk = 48;
  for (size_t I = 0; I < Data.size(); I += MaxChunk) {
    SmallString<256> Str;
    raw_svector_ostream OS(Str);
    OS << "\tDEFM\t\"";
    for (unsigned char C : Data.substr(I, MaxChunk)) {
      if (isPrint(C) && C != '"' && C != '\\')
        OS << C;
      else
        OS << '\\' << format("%03o", C);
    }
    OS << '"';
    getStreamer().emitRawText(OS.str());
  }
}

/// Picks the e_flags CPU value for the subtarget.
static unsigned getEFlagsForFeatureSet(const FeatureBitset &Features) {
  if (Features[Z80::FeatureSM83])
    return ELF::EF_Z80_MACH_SM83;
  return ELF::EF_Z80_MACH_Z80;
}

Z80TargetELFStreamer::Z80TargetELFStreamer(MCStreamer &S,
                                           const MCSubtargetInfo &STI)
    : Z80TargetStreamer(S) {
  ELFObjectWriter &W = getStreamer().getWriter();
  unsigned EFlags = W.getELFHeaderEFlags() & ~ELF::EF_Z80_MACH_MSK;
  EFlags |= getEFlagsForFeatureSet(STI.getFeatureBits());
  W.setELFHeaderEFlags(EFlags);
}

MCELFStreamer &Z80TargetELFStreamer::getStreamer() {
  return static_cast<MCELFStreamer &>(Streamer);
}

} // end namespace llvm
