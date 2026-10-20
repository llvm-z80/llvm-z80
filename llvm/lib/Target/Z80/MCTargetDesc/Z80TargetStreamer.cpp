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
#include "llvm/MC/MCSymbolELF.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Format.h"

namespace llvm {

Z80TargetStreamer::Z80TargetStreamer(MCStreamer &S) : MCTargetStreamer(S) {}

void Z80TargetStreamer::finish() {
  // Non-ELF targets (sdasz80 and z88dk) have their own CRT initialization.
  const MCAsmInfo &MAI = Streamer.getContext().getAsmInfo();
  if (MAI.isSDCC() || MAI.isZ88DK())
    return;
  if (hasBSS())
    stronglyReference("__do_zero_bss",
                      "Declaring this symbol tells the CRT that there is "
                      "something in .bss, so it may need to be zeroed.");

  if (hasData())
    stronglyReference(
        "__do_copy_data",
        "Declaring this symbol tells the CRT that there is something in .data, "
        "so it may need to be copied from LMA to VMA.");

  if (hasInitArray())
    stronglyReference("__do_init_array",
                      "Declaring this symbol tells the CRT that there are "
                      "initialization routines to be run in .init_array");

  if (hasFiniArray())
    stronglyReference("__do_fini_array",
                      "Declaring this symbol tells the CRT that there are "
                      "finalization routines to be run in .fini_array");
}

void Z80TargetStreamer::stronglyReference(StringRef Name, StringRef Comment) {
  MCStreamer &OS = getStreamer();
  MCContext &Context = OS.getContext();
  MCSymbol *Sym = Context.getOrCreateSymbol(Name);
  OS.emitRawComment(Comment);
  stronglyReference(Sym);
}

static bool HasPrefix(StringRef Name, StringRef Prefix) {
  SmallString<32> PrefixDot = Prefix;
  PrefixDot += ".";
  return Name == Prefix || Name.starts_with(PrefixDot);
}

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
  HasBSS |= HasPrefix(Section->getName(), ".bss");
  HasData |= HasPrefix(Section->getName(), ".data");
  HasInitArray |= HasPrefix(Section->getName(), ".init_array");
  HasFiniArray |= HasPrefix(Section->getName(), ".fini_array");
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

void Z80TargetAsmStreamer::stronglyReference(MCSymbol *Sym) {
  getStreamer().emitSymbolAttribute(Sym, MCSA_Global);
}

/// Picks the e_flags CPU value for the subtarget. Z180, R800 and eZ80 also set
/// the Z80 feature, so they are checked before falling back to Z80.
static unsigned getEFlagsForFeatureSet(const FeatureBitset &Features) {
  if (Features[Z80::FeatureSM83])
    return ELF::EF_Z80_MACH_SM83;
  if (Features[Z80::FeatureEZ80])
    return ELF::EF_Z80_MACH_EZ80_Z80;
  if (Features[Z80::FeatureR800])
    return ELF::EF_Z80_MACH_R800;
  if (Features[Z80::FeatureZ180])
    return ELF::EF_Z80_MACH_Z180;
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

void Z80TargetELFStreamer::changeSection(const MCSection *CurSection,
                                         MCSection *Section,
                                         uint32_t Subsection, raw_ostream &OS) {
  MCTargetStreamer::changeSection(CurSection, Section, Subsection, OS);
  HasBSS |= HasPrefix(Section->getName(), ".bss");
  HasData |= HasPrefix(Section->getName(), ".data");
  HasInitArray |= HasPrefix(Section->getName(), ".init_array");
  HasFiniArray |= HasPrefix(Section->getName(), ".fini_array");
}

void Z80TargetELFStreamer::stronglyReference(MCSymbol *Sym) {
  MCELFStreamer &ELFStreamer = getStreamer();

  ELFStreamer.emitSymbolAttribute(Sym, MCSA_Global);

  // Mark the symbol as strongly referenced.
  MCSymbolELF &SymELF = static_cast<MCSymbolELF &>(*Sym);
  SymELF.setOther(ELF::STO_Z80_STRONG_REF);
}

MCELFStreamer &Z80TargetELFStreamer::getStreamer() {
  return static_cast<MCELFStreamer &>(Streamer);
}

} // end namespace llvm
