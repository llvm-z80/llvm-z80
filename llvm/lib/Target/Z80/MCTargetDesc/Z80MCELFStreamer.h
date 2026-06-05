//===--------- Z80MCELFStreamer.h - Z80 subclass of MCELFStreamer ---------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_Z80_MCTARGETDESC_Z80MCELFSTREAMER_H
#define LLVM_LIB_TARGET_Z80_MCTARGETDESC_Z80MCELFSTREAMER_H

#include "MCTargetDesc/Z80MCExpr.h"
#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCELFStreamer.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCObjectWriter.h"

namespace llvm {

class Z80MCELFStreamer : public MCELFStreamer {
  std::unique_ptr<MCInstrInfo> MCII;

public:
  Z80MCELFStreamer(MCContext &Context, std::unique_ptr<MCAsmBackend> TAB,
                   std::unique_ptr<MCObjectWriter> OW,
                   std::unique_ptr<MCCodeEmitter> Emitter)
      : MCELFStreamer(Context, std::move(TAB), std::move(OW),
                      std::move(Emitter)),
        MCII(createZ80MCInstrInfo()) {}

  void initSections(bool NoExecStack, const MCSubtargetInfo &STI) override;
  void changeSection(MCSection *Section, uint32_t Subsection = 0) override;

  void emitInstruction(const MCInst &Inst, const MCSubtargetInfo &STI) override;

  void emitValueImpl(const MCExpr *Value, unsigned Size,
                     SMLoc Loc = SMLoc()) override;

  void emitMosAddrAsciz(const MCExpr *Value, unsigned Size,
                        SMLoc Loc = SMLoc());

  bool hasBSS() const { return HasBSS; }
  bool hasData() const { return HasData; }
  bool hasInitArray() const { return HasInitArray; }
  bool hasFiniArray() const { return HasFiniArray; }

private:
  bool HasBSS = false;
  bool HasData = false;
  bool HasInitArray = false;
  bool HasFiniArray = false;
};

MCStreamer *createZ80MCELFStreamer(const Triple &T, MCContext &Ctx,
                                   std::unique_ptr<MCAsmBackend> &&TAB,
                                   std::unique_ptr<MCObjectWriter> &&OW,
                                   std::unique_ptr<MCCodeEmitter> &&Emitter);

} // end namespace llvm

#endif // LLVM_LIB_TARGET_Z80_MCTARGETDESC_Z80MCELFSTREAMER_H
