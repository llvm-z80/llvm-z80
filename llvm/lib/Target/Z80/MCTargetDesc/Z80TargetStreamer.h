//===-- Z80TargetStreamer.h - Z80 Target Streamer --------------*- C++ -*--===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_Z80_TARGET_STREAMER_H
#define LLVM_Z80_TARGET_STREAMER_H

#include "llvm/MC/MCELFStreamer.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/FormattedStream.h"

namespace llvm {
class MCStreamer;

/// A generic Z80 target output stream.
class Z80TargetStreamer : public MCTargetStreamer {
public:
  explicit Z80TargetStreamer(MCStreamer &S);

  void finish() override;

protected:
  virtual bool hasBSS() = 0;
  virtual bool hasData() = 0;
  virtual bool hasInitArray() = 0;
  virtual bool hasFiniArray() = 0;

  void stronglyReference(StringRef Name, StringRef Comment);
  virtual void stronglyReference(MCSymbol *Sym) = 0;
};

/// A target streamer for textual Z80 assembly code.
class Z80TargetAsmStreamer final : public Z80TargetStreamer {
public:
  Z80TargetAsmStreamer(MCStreamer &S, formatted_raw_ostream &OS);

private:
  formatted_raw_ostream &OS;

  void changeSection(const MCSection *CurSection, MCSection *Section,
                     uint32_t SubSection, raw_ostream &OS) override;

  bool hasBSS() override { return HasBSS; }
  bool hasData() override { return HasData; }
  bool hasInitArray() override { return HasInitArray; }
  bool hasFiniArray() override { return HasFiniArray; }

  void stronglyReference(MCSymbol *Sym) override;

  bool HasBSS = false;
  bool HasData = false;
  bool HasInitArray = false;
  bool HasFiniArray = false;
};

inline Z80TargetAsmStreamer::Z80TargetAsmStreamer(MCStreamer &S,
                                                  formatted_raw_ostream &OS)
    : Z80TargetStreamer(S), OS(OS) {}

/// A target streamer for a Z80 ELF object file.
class Z80TargetELFStreamer final : public Z80TargetStreamer {
public:
  Z80TargetELFStreamer(MCStreamer &S, const MCSubtargetInfo &STI);

private:
  MCELFStreamer &getStreamer();

  void changeSection(const MCSection *CurSection, MCSection *Section,
                     uint32_t Subsection, raw_ostream &OS) override;

  bool hasBSS() override { return HasBSS; }
  bool hasData() override { return HasData; }
  bool hasInitArray() override { return HasInitArray; }
  bool hasFiniArray() override { return HasFiniArray; }

  void stronglyReference(MCSymbol *Sym) override;

  bool HasBSS = false;
  bool HasData = false;
  bool HasInitArray = false;
  bool HasFiniArray = false;
};

} // end namespace llvm

#endif // LLVM_Z80_TARGET_STREAMER_H
