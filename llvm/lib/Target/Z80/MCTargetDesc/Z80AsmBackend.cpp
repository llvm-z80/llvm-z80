//===-- Z80AsmBackend.cpp - Z80 Asm Backend  ------------------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the Z80AsmBackend class.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/Z80AsmBackend.h"
#include "MCTargetDesc/Z80ELFObjectWriter.h"
#include "MCTargetDesc/Z80FixupKinds.h"
#include "MCTargetDesc/Z80MCTargetDesc.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/ErrorHandling.h"

#include <cstdint>
#include <memory>

namespace llvm {

MCAsmBackend *createZ80AsmBackend(const Target &T, const MCSubtargetInfo &STI,
                                  const MCRegisterInfo &MRI,
                                  const llvm::MCTargetOptions &TO) {
  return new Z80AsmBackend(STI.getTargetTriple().getOS());
}

std::unique_ptr<MCObjectTargetWriter>
Z80AsmBackend::createObjectTargetWriter() const {
  return createZ80ELFObjectWriter(ELF::ELFOSABI_NONE);
}

bool Z80AsmBackend::fixupNeedsRelaxationAdvanced(const MCFragment &F,
                                                 const MCFixup &Fixup,
                                                 const MCValue &Target,
                                                 uint64_t Value,
                                                 bool Resolved) const {
  // Primary JR→JP relaxation is handled by BranchRelaxation + Z80BranchCleanup
  // at the MachineFunction level. This check serves as a safety net for cases
  // that bypass BranchRelaxation (inline assembly, late pseudo expansion).
  if (!Resolved)
    return true;

  if (Fixup.getKind() == (MCFixupKind)Z80::PCRel8) {
    int64_t SignedValue = static_cast<int64_t>(Value);
    return SignedValue < -128 || SignedValue > 127;
  }

  return false;
}

MCFixupKindInfo Z80AsmBackend::getFixupKindInfo(MCFixupKind Kind) const {
  static const MCFixupKindInfo Infos[] = {
      // name                offset bits flags
      {"Imm8", 0, 8, 0},
      {"Imm16", 0, 16, 0},
      {"Addr8", 0, 8, 0},
      {"Addr16", 0, 16, 0},
      {"Addr16_Low", 0, 8, 0},
      {"Addr16_High", 0, 8, 0},
      {"Addr24", 0, 24, 0},
      {"Addr24_Bank", 0, 8, 0},
      {"Addr24_Segment", 0, 16, 0},
      {"Addr24_Segment_Low", 0, 8, 0},
      {"Addr24_Segment_High", 0, 8, 0},
      {"Addr13", 0, 13, 0},
      {"PCRel8", 0, 8, 0},
      {"PCRel16", 0, 16, 0},
      {"AddrAsciz", 0, 16, 0},
      {"Disp8", 0, 8, 0}, // 8-bit signed displacement for indexed addressing
      {"LDH8", 0, 8, 0},  // SM83 LDH operand: 8-bit offset from 0xFF00
  };

  if (Kind < FirstTargetFixupKind)
    return MCAsmBackend::getFixupKindInfo(Kind);

  unsigned Index = Kind - FirstTargetFixupKind;
  if (Index < Z80::NumTargetFixupKinds)
    return Infos[Index];

  llvm_unreachable("Invalid fixup kind!");
}

void Z80AsmBackend::applyFixup(const MCFragment &F, const MCFixup &Fixup,
                               const MCValue &Target, uint8_t *Data,
                               uint64_t Value, bool IsResolved) {
  // For PC-relative fixups, LLVM computes Value = target - fixup_byte_addr.
  // Z80 JR displacement is relative to the end of the instruction (PC+2),
  // but the fixup byte is at PC+1, so we need to subtract 1.
  // Note: lld applies the same -1 adjustment in Z80::relocate() for
  // link-time relocations; this adjustment handles resolved (internal) fixups.
  if (Fixup.isPCRel())
    Value -= 1;

  // SM83 LDH: subtract 0xFF00 base to get the 8-bit offset
  if (Fixup.getKind() == Z80::LDH8)
    Value -= 0xFF00;

  maybeAddReloc(F, Fixup, Target, Value, IsResolved);

  unsigned Kind = Fixup.getKind();
  unsigned NumBytes = 0;

  switch (Kind) {
  default:
    llvm_unreachable("Unknown fixup kind!");
  case FK_Data_1:
  case Z80::Imm8:
  case Z80::Addr8:
  case Z80::Addr16_Low:
  case Z80::Addr16_High:
  case Z80::Addr24_Bank:
  case Z80::Addr24_Segment_Low:
  case Z80::Addr24_Segment_High:
  case Z80::PCRel8:
  case Z80::Disp8: // 8-bit signed displacement for indexed addressing
  case Z80::LDH8: // SM83 LDH operand: 8-bit offset from 0xFF00
    NumBytes = 1;
    break;
  case FK_Data_2:
  case Z80::Addr16:
  case Z80::Addr24_Segment:
  case Z80::PCRel16:
  case Z80::AddrAsciz:
  case Z80::Imm16:
  case Z80::Addr13:
    NumBytes = 2;
    break;
  case Z80::Addr24:
    NumBytes = 3;
    break;
  case FK_Data_4:
    NumBytes = 4;
    break;
  case FK_Data_8:
    NumBytes = 8;
    break;
  }

  // Write the value in little-endian format
  // Note: Data already points to the correct location within the fragment
  // (caller adds Fixup.getOffset() before passing Data to us)
  for (unsigned i = 0; i < NumBytes; ++i) {
    Data[i] = static_cast<uint8_t>(Value & 0xFF);
    Value >>= 8;
  }
}

bool Z80AsmBackend::mayNeedRelaxation(unsigned Opcode,
                                      ArrayRef<MCOperand> Operands,
                                      const MCSubtargetInfo &STI) const {
  // Primary JR→JP relaxation is handled at the MachineFunction level by
  // BranchRelaxation + Z80BranchCleanup (see Z80BranchCleanup.cpp).
  // This AsmBackend implementation serves as a safety net for edge cases
  // that bypass BranchRelaxation (e.g. inline assembly, post-relaxation
  // pseudo expansion).
  switch (Opcode) {
  case Z80::JR_e:
  case Z80::JR_Z_e:
  case Z80::JR_NZ_e:
  case Z80::JR_C_e:
  case Z80::JR_NC_e:
    return true;
  default:
    return false;
  }
}

void Z80AsmBackend::relaxInstruction(MCInst &Inst,
                                     const MCSubtargetInfo &STI) const {
  // Relax JR (2 bytes, PC-relative) → JP (3 bytes, absolute).
  // Normally BranchRelaxation handles this at the MachineFunction level,
  // but this fallback catches edge cases (inline asm, late expansions).
  unsigned NewOpcode;
  switch (Inst.getOpcode()) {
  case Z80::JR_e:    NewOpcode = Z80::JP_nn; break;
  case Z80::JR_Z_e:  NewOpcode = Z80::JP_Z_nn; break;
  case Z80::JR_NZ_e: NewOpcode = Z80::JP_NZ_nn; break;
  case Z80::JR_C_e:  NewOpcode = Z80::JP_C_nn; break;
  case Z80::JR_NC_e: NewOpcode = Z80::JP_NC_nn; break;
  default:
    llvm_unreachable("Unexpected instruction to relax");
  }
  Inst.setOpcode(NewOpcode);
}

unsigned Z80AsmBackend::relaxInstructionTo(unsigned Opcode,
                                           const MCSubtargetInfo &STI,
                                           bool &BankRelax) {
  // Z80 doesn't have the same relaxation model as the original architecture
  BankRelax = false;
  return 0;
}

void Z80AsmBackend::translateOpcodeToSubtarget(MCInst &Inst,
                                               const MCSubtargetInfo &STI) {
  // No subtarget-specific opcode translation needed for Z80
}

void Z80AsmBackend::relaxForImmediate(MCInst &Inst,
                                      const MCSubtargetInfo &STI) {
  // No immediate relaxation needed for Z80
}

bool Z80AsmBackend::writeNopData(raw_ostream &OS, uint64_t Count,
                                 const MCSubtargetInfo *STI) const {
  // Z80 NOP is 0x00
  for (uint64_t i = 0; i < Count; ++i)
    OS << char(0x00);
  return true;
}

} // end namespace llvm
