//===-- Z80InstrInfo.cpp - Z80 Instruction Information --------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the Z80 implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#include "Z80InstrInfo.h"

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80OpcodeUtils.h"
#include "Z80RegisterInfo.h"
#include "Z80Subtarget.h"

#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "z80-instrinfo"

#define GET_INSTRINFO_CTOR_DTOR
#include "Z80GenInstrInfo.inc"

Z80InstrInfo::Z80InstrInfo(const Z80Subtarget &STI)
    : Z80GenInstrInfo(STI, *STI.getRegisterInfo(),
                      /*CFSetupOpcode=*/Z80::ADJCALLSTACKDOWN,
                      /*CFDestroyOpcode=*/Z80::ADJCALLSTACKUP),
      STI(&STI) {}

Register Z80InstrInfo::isLoadFromStackSlot(const MachineInstr &MI,
                                           int &FrameIndex) const {
  switch (MI.getOpcode()) {
  case Z80::RELOAD_GR8:
  case Z80::RELOAD_GR16:
    if (MI.getOperand(1).isFI()) {
      FrameIndex = MI.getOperand(1).getIndex();
      return MI.getOperand(0).getReg();
    }
    break;
  }
  return 0;
}

Register Z80InstrInfo::isStoreToStackSlot(const MachineInstr &MI,
                                          int &FrameIndex) const {
  switch (MI.getOpcode()) {
  case Z80::SPILL_GR8:
  case Z80::SPILL_GR16:
    if (MI.getOperand(1).isFI()) {
      FrameIndex = MI.getOperand(1).getIndex();
      return MI.getOperand(0).getReg();
    }
    break;
  }
  return 0;
}

// Get opcode for LD r,n (load immediate to 8-bit register)
static unsigned getLoadImmR8Opcode(Register Reg) {
  static const unsigned Opcodes[] = {Z80::LD_A_n, Z80::LD_B_n, Z80::LD_C_n,
                                     Z80::LD_D_n, Z80::LD_E_n, Z80::LD_H_n,
                                     Z80::LD_L_n};
  int Idx = Z80::gr8RegToIndex(Reg);
  return Idx >= 0 ? Opcodes[Idx] : 0;
}

static unsigned getSUBOpcode(Register Reg) {
  static const unsigned T[] = {Z80::SUB_A, Z80::SUB_B, Z80::SUB_C, Z80::SUB_D,
                               Z80::SUB_E, Z80::SUB_H, Z80::SUB_L};
  int I = Z80::gr8RegToIndex(Reg);
  return I >= 0 ? T[I] : 0;
}

static unsigned getSBCOpcode(Register Reg) {
  static const unsigned T[] = {Z80::SBC_A_A, Z80::SBC_A_B, Z80::SBC_A_C,
                               Z80::SBC_A_D, Z80::SBC_A_E, Z80::SBC_A_H,
                               Z80::SBC_A_L};
  int I = Z80::gr8RegToIndex(Reg);
  return I >= 0 ? T[I] : 0;
}

static unsigned getADCOpcode(Register Reg) {
  static const unsigned T[] = {Z80::ADC_A_A, Z80::ADC_A_B, Z80::ADC_A_C,
                               Z80::ADC_A_D, Z80::ADC_A_E, Z80::ADC_A_H,
                               Z80::ADC_A_L};
  int I = Z80::gr8RegToIndex(Reg);
  return I >= 0 ? T[I] : 0;
}

static unsigned getADD8Opcode(Register Reg) {
  static const unsigned T[] = {Z80::ADD_A_A, Z80::ADD_A_B, Z80::ADD_A_C,
                               Z80::ADD_A_D, Z80::ADD_A_E, Z80::ADD_A_H,
                               Z80::ADD_A_L};
  int I = Z80::gr8RegToIndex(Reg);
  return I >= 0 ? T[I] : 0;
}

// Get low and high 8-bit sub-registers of a 16-bit register pair.
static std::pair<Register, Register> getSubRegs16(Register Reg) {
  switch (Reg.id()) {
  case Z80::BC:
    return {Z80::C, Z80::B};
  case Z80::DE:
    return {Z80::E, Z80::D};
  case Z80::HL:
    return {Z80::L, Z80::H};
  default:
    llvm_unreachable("Not a GR16 register pair");
  }
}

// Get PUSH opcode for a 16-bit register
static unsigned getPUSHOpcode(Register Reg) { return Z80::getPushOpcode(Reg); }

static unsigned getPOPOpcode(Register Reg) { return Z80::getPopOpcode(Reg); }

void Z80InstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                               MachineBasicBlock::iterator I,
                               const DebugLoc &DL, Register DestReg,
                               Register SrcReg, bool KillSrc,
                               bool RenamableDest, bool RenamableSrc) const {
  // Handle 8-bit register copies: LD r,r'
  if (Z80::GR8RegClass.contains(DestReg) && Z80::GR8RegClass.contains(SrcReg)) {
    unsigned Opcode = Z80::getLD8RegOpcode(DestReg, SrcReg);
    if (Opcode) {
      BuildMI(MBB, I, DL, get(Opcode));
      return;
    }
  }

  // EX DE,HL: single-byte swap for DE<->HL copies (Z80 only).
  // Note: EX DE,HL swaps both registers, so the source is also modified.
  // This is safe when the source is dead after this copy.
  // SM83 lacks EX DE,HL; fall through to the 2x LD path below.
  if (!STI->hasSM83() && ((DestReg == Z80::DE && SrcReg == Z80::HL) ||
                          (DestReg == Z80::HL && SrcReg == Z80::DE))) {
    bool SrcDead = KillSrc;
    if (!SrcDead) {
      // KillSrc may not be set for physical register liveins.
      // Check actual liveness after this point.
      auto Next = std::next(I);
      auto LQR =
          MBB.computeRegisterLiveness(STI->getRegisterInfo(), SrcReg, Next);
      SrcDead = (LQR == MachineBasicBlock::LQR_Dead);
    }
    if (SrcDead) {
      BuildMI(MBB, I, DL, get(Z80::EX_DE_HL));
      return;
    }
  }

  // Handle 16-bit register copies between BC, DE, HL using two 8-bit LDs.
  // LD r,r' is 1 byte / 4 cycles each (2 bytes / 8 cycles total),
  // much faster than PUSH/POP (2 bytes / 21 cycles).
  if (Z80::GR16RegClass.contains(DestReg) &&
      Z80::GR16RegClass.contains(SrcReg)) {
    const TargetRegisterInfo *TRI = STI->getRegisterInfo();
    Register DstLo = TRI->getSubReg(DestReg, Z80::sub_lo);
    Register DstHi = TRI->getSubReg(DestReg, Z80::sub_hi);
    Register SrcLo = TRI->getSubReg(SrcReg, Z80::sub_lo);
    Register SrcHi = TRI->getSubReg(SrcReg, Z80::sub_hi);
    if (DstLo && DstHi && SrcLo && SrcHi) {
      unsigned LoOp = Z80::getLD8RegOpcode(DstLo, SrcLo);
      unsigned HiOp = Z80::getLD8RegOpcode(DstHi, SrcHi);
      if (LoOp && HiOp) {
        BuildMI(MBB, I, DL, get(LoOp));
        BuildMI(MBB, I, DL, get(HiOp));
        return;
      }
    }
  }

  // Handle 16-bit register copies involving IX/IY using PUSH/POP sequence.
  // IX/IY have no 8-bit sub-registers and no direct LD rr,rr' instruction.
  if ((Z80::GR16RegClass.contains(DestReg) ||
       Z80::IR16RegClass.contains(DestReg)) &&
      (Z80::GR16RegClass.contains(SrcReg) ||
       Z80::IR16RegClass.contains(SrcReg))) {
    unsigned PushOp = getPUSHOpcode(SrcReg);
    unsigned PopOp = getPOPOpcode(DestReg);
    if (PushOp && PopOp) {
      BuildMI(MBB, I, DL, get(PushOp));
      BuildMI(MBB, I, DL, get(PopOp));
      return;
    }
  }

  // Handle SP loads from HL, IX, IY
  if (DestReg == Z80::SP) {
    switch (SrcReg.id()) {
    case Z80::HL:
      BuildMI(MBB, I, DL, get(Z80::LD_SP_HL));
      return;
    case Z80::IX:
      BuildMI(MBB, I, DL, get(Z80::LD_SP_IX));
      return;
    case Z80::IY:
      BuildMI(MBB, I, DL, get(Z80::LD_SP_IY));
      return;
    default:
      // BC/DE → SP: route through HL (LD HL,src; LD SP,HL)
      // HL is clobbered, but stackrestore is typically at scope exit
      // where HL is dead.
      copyPhysReg(MBB, I, DL, Z80::HL, SrcReg, KillSrc);
      BuildMI(MBB, I, DL, get(Z80::LD_SP_HL));
      return;
    }
  }

  // Handle reading SP into HL, IX, IY
  // Z80 has no "LD HL,SP" so we use "LD reg,0; ADD reg,SP"
  // ADD rr,SP clobbers FLAGS (carry). If FLAGS is live, we wrap with
  // PUSH AF/POP AF and compensate the offset (+2) for the changed SP.
  if (SrcReg == Z80::SP) {
    const TargetRegisterInfo *TRI = STI->getRegisterInfo();
    auto FlagsLQ = MBB.computeRegisterLiveness(TRI, Z80::FLAGS, I);
    bool FlagsLive = (FlagsLQ != MachineBasicBlock::LQR_Dead);
    int SPComp = FlagsLive ? 2 : 0; // SP compensation for PUSH AF

    if (DestReg == Z80::HL) {
      if (FlagsLive)
        BuildMI(MBB, I, DL, get(Z80::PUSH_AF));
      BuildMI(MBB, I, DL, get(Z80::LD_HL_nn)).addImm(SPComp);
      BuildMI(MBB, I, DL, get(Z80::ADD_HL_SP));
      if (FlagsLive)
        BuildMI(MBB, I, DL, get(Z80::POP_AF));
      return;
    }
    if (DestReg == Z80::IX) {
      if (FlagsLive)
        BuildMI(MBB, I, DL, get(Z80::PUSH_AF));
      BuildMI(MBB, I, DL, get(Z80::LD_IX_nn)).addImm(SPComp);
      BuildMI(MBB, I, DL, get(Z80::ADD_IX_SP));
      if (FlagsLive)
        BuildMI(MBB, I, DL, get(Z80::POP_AF));
      return;
    }
    if (DestReg == Z80::IY) {
      if (FlagsLive)
        BuildMI(MBB, I, DL, get(Z80::PUSH_AF));
      BuildMI(MBB, I, DL, get(Z80::LD_IY_nn)).addImm(SPComp);
      BuildMI(MBB, I, DL, get(Z80::ADD_IY_SP));
      if (FlagsLive)
        BuildMI(MBB, I, DL, get(Z80::POP_AF));
      return;
    }
    // SP → BC or DE: Z80 has no ADD BC,SP / ADD DE,SP, so route through HL.
    // PUSH HL; LD HL,N; ADD HL,SP; LD r,H; LD r,L; POP HL
    // N compensates for PUSH HL (and PUSH AF if FLAGS is live).
    if (DestReg == Z80::BC || DestReg == Z80::DE) {
      unsigned LdHiOp = (DestReg == Z80::BC) ? Z80::LD_B_H : Z80::LD_D_H;
      unsigned LdLoOp = (DestReg == Z80::BC) ? Z80::LD_C_L : Z80::LD_E_L;
      if (FlagsLive)
        BuildMI(MBB, I, DL, get(Z80::PUSH_AF));
      BuildMI(MBB, I, DL, get(Z80::PUSH_HL));
      BuildMI(MBB, I, DL, get(Z80::LD_HL_nn)).addImm(SPComp + 2);
      BuildMI(MBB, I, DL, get(Z80::ADD_HL_SP));
      BuildMI(MBB, I, DL, get(LdHiOp));
      BuildMI(MBB, I, DL, get(LdLoOp));
      BuildMI(MBB, I, DL, get(Z80::POP_HL));
      if (FlagsLive)
        BuildMI(MBB, I, DL, get(Z80::POP_AF));
      return;
    }
  }

  // Handle 8-bit copies FROM IXH/IXL/IYH/IYL to a GR8 register.
  // These are undocumented Z80 registers (DD/FD-prefixed H/L opcodes).
  // Route through PUSH IX/IY; POP HL to extract the byte.
  {
    bool SrcIsIXH = (SrcReg == Z80::IXH), SrcIsIXL = (SrcReg == Z80::IXL);
    bool SrcIsIYH = (SrcReg == Z80::IYH), SrcIsIYL = (SrcReg == Z80::IYL);
    bool SrcIsIndexHi = SrcIsIXH || SrcIsIYH;
    bool SrcIsIndexLo = SrcIsIXL || SrcIsIYL;

    if ((SrcIsIndexHi || SrcIsIndexLo) && Z80::GR8RegClass.contains(DestReg)) {
      unsigned PushOp = (SrcIsIXH || SrcIsIXL) ? Z80::PUSH_IX : Z80::PUSH_IY;
      Register ExtractReg = SrcIsIndexHi ? Z80::H : Z80::L;

      if (DestReg == Z80::H || DestReg == Z80::L) {
        Register OtherReg = (DestReg == Z80::H) ? Z80::L : Z80::H;
        const TargetRegisterInfo *TRI = STI->getRegisterInfo();
        auto OtherLQ = MBB.computeRegisterLiveness(TRI, OtherReg, I);

        if (OtherLQ == MachineBasicBlock::LQR_Dead) {
          BuildMI(MBB, I, DL, get(PushOp));
          BuildMI(MBB, I, DL, get(Z80::POP_HL));
          if ((DestReg == Z80::H && SrcIsIndexLo) ||
              (DestReg == Z80::L && SrcIsIndexHi)) {
            unsigned LdOp = Z80::getLD8RegOpcode(DestReg, ExtractReg);
            BuildMI(MBB, I, DL, get(LdOp));
          }
        } else {
          Register ScratchReg = SrcIsIndexHi ? Z80::D : Z80::E;
          BuildMI(MBB, I, DL, get(Z80::PUSH_DE));
          BuildMI(MBB, I, DL, get(PushOp));
          BuildMI(MBB, I, DL, get(Z80::POP_DE));
          unsigned LdOp = Z80::getLD8RegOpcode(DestReg, ScratchReg);
          BuildMI(MBB, I, DL, get(LdOp));
          BuildMI(MBB, I, DL, get(Z80::POP_DE));
        }
      } else {
        BuildMI(MBB, I, DL, get(Z80::PUSH_HL));
        BuildMI(MBB, I, DL, get(PushOp));
        BuildMI(MBB, I, DL, get(Z80::POP_HL));
        unsigned LdOp = Z80::getLD8RegOpcode(DestReg, ExtractReg);
        BuildMI(MBB, I, DL, get(LdOp));
        BuildMI(MBB, I, DL, get(Z80::POP_HL));
      }
      return;
    }
  }

  // Handle 8-bit copies TO IXH/IXL/IYH/IYL from a GR8 register.
  // Route through HL: save HL, PUSH IX/IY; POP HL, modify, PUSH HL; POP IX/IY.
  {
    bool DstIsIXH = (DestReg == Z80::IXH), DstIsIXL = (DestReg == Z80::IXL);
    bool DstIsIYH = (DestReg == Z80::IYH), DstIsIYL = (DestReg == Z80::IYL);
    bool DstIsIndexHi = DstIsIXH || DstIsIYH;
    bool DstIsIndexLo = DstIsIXL || DstIsIYL;

    if ((DstIsIndexHi || DstIsIndexLo) && Z80::GR8RegClass.contains(SrcReg)) {
      unsigned PushIR = (DstIsIXH || DstIsIXL) ? Z80::PUSH_IX : Z80::PUSH_IY;
      unsigned PopIR = (DstIsIXH || DstIsIXL) ? Z80::POP_IX : Z80::POP_IY;
      Register TargetReg = DstIsIndexHi ? Z80::H : Z80::L;

      if (SrcReg == Z80::H || SrcReg == Z80::L) {
        BuildMI(MBB, I, DL, get(Z80::PUSH_AF));
        unsigned LdASrc = Z80::getLD8RegOpcode(Z80::A, SrcReg);
        BuildMI(MBB, I, DL, get(LdASrc));
        BuildMI(MBB, I, DL, get(Z80::PUSH_HL));
        BuildMI(MBB, I, DL, get(PushIR));
        BuildMI(MBB, I, DL, get(Z80::POP_HL));
        unsigned LdTargetA = Z80::getLD8RegOpcode(TargetReg, Z80::A);
        BuildMI(MBB, I, DL, get(LdTargetA));
        BuildMI(MBB, I, DL, get(Z80::PUSH_HL));
        BuildMI(MBB, I, DL, get(PopIR));
        BuildMI(MBB, I, DL, get(Z80::POP_HL));
        BuildMI(MBB, I, DL, get(Z80::POP_AF));
        return;
      }

      BuildMI(MBB, I, DL, get(Z80::PUSH_HL));
      BuildMI(MBB, I, DL, get(PushIR));
      BuildMI(MBB, I, DL, get(Z80::POP_HL));
      unsigned LdOp = Z80::getLD8RegOpcode(TargetReg, SrcReg);
      BuildMI(MBB, I, DL, get(LdOp));
      BuildMI(MBB, I, DL, get(Z80::PUSH_HL));
      BuildMI(MBB, I, DL, get(PopIR));
      BuildMI(MBB, I, DL, get(Z80::POP_HL));
      return;
    }
  }

  llvm_unreachable("Cannot copy between these registers");
}

// Get the indexed store opcode for LD (IX+d),r
static unsigned getStoreIXdOpcode(Register Reg) {
  static const unsigned T[] = {Z80::LD_IXd_A, Z80::LD_IXd_B, Z80::LD_IXd_C,
                               Z80::LD_IXd_D, Z80::LD_IXd_E, Z80::LD_IXd_H,
                               Z80::LD_IXd_L};
  int I = Z80::gr8RegToIndex(Reg);
  return I >= 0 ? T[I] : 0;
}

static unsigned getLoadIXdOpcode(Register Reg) {
  static const unsigned T[] = {Z80::LD_A_IXd, Z80::LD_B_IXd, Z80::LD_C_IXd,
                               Z80::LD_D_IXd, Z80::LD_E_IXd, Z80::LD_H_IXd,
                               Z80::LD_L_IXd};
  int I = Z80::gr8RegToIndex(Reg);
  return I >= 0 ? T[I] : 0;
}

void Z80InstrInfo::storeRegToStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI, Register SrcReg,
    bool isKill, int FrameIndex, const TargetRegisterClass *RC, Register VReg,
    MachineInstr::MIFlag Flags) const {
  DebugLoc DL;
  if (MI != MBB.end())
    DL = MI->getDebugLoc();

  MachineFunction &MF = *MBB.getParent();
  MachineFrameInfo &MFI = MF.getFrameInfo();

  // Always use SPILL pseudos for both physical and virtual registers.
  // The pseudos handle large frame offsets correctly (HL-indirect addressing)
  // and are expanded in expandPostRAPseudo after eliminateFrameIndex.
  if (Z80::GR8RegClass.hasSubClassEq(RC)) {
    BuildMI(MBB, MI, DL, get(Z80::SPILL_GR8))
        .addReg(SrcReg, getKillRegState(isKill))
        .addFrameIndex(FrameIndex)
        .addImm(0)
        .addMemOperand(MF.getMachineMemOperand(
            MachinePointerInfo::getFixedStack(MF, FrameIndex),
            MachineMemOperand::MOStore, 1, MFI.getObjectAlign(FrameIndex)));
    return;
  }

  {
    const TargetRegisterInfo *TRI = STI->getRegisterInfo();
    if (RC->hasSuperClassEq(&Z80::GR16RegClass) ||
        TRI->getCommonSubClass(RC, &Z80::GR16RegClass) ||
        Z80::IR16RegClass.hasSubClassEq(RC)) {
      BuildMI(MBB, MI, DL, get(Z80::SPILL_GR16))
          .addReg(SrcReg, getKillRegState(isKill))
          .addFrameIndex(FrameIndex)
          .addImm(0)
          .addMemOperand(MF.getMachineMemOperand(
              MachinePointerInfo::getFixedStack(MF, FrameIndex),
              MachineMemOperand::MOStore, 2, MFI.getObjectAlign(FrameIndex)));
      return;
    }
  }

  llvm_unreachable("storeRegToStackSlot: unsupported register class");
}

void Z80InstrInfo::loadRegFromStackSlot(MachineBasicBlock &MBB,
                                        MachineBasicBlock::iterator MI,
                                        Register DestReg, int FrameIndex,
                                        const TargetRegisterClass *RC,
                                        Register VReg, unsigned SubReg,
                                        MachineInstr::MIFlag Flags) const {
  DebugLoc DL;
  if (MI != MBB.end())
    DL = MI->getDebugLoc();

  MachineFunction &MF = *MBB.getParent();
  MachineFrameInfo &MFI = MF.getFrameInfo();

  // Always use RELOAD pseudos for both physical and virtual registers.
  // The pseudos handle large frame offsets correctly (HL-indirect addressing)
  // and are expanded in expandPostRAPseudo after eliminateFrameIndex.
  if (Z80::GR8RegClass.hasSubClassEq(RC)) {
    BuildMI(MBB, MI, DL, get(Z80::RELOAD_GR8))
        .addReg(DestReg, RegState::Define)
        .addFrameIndex(FrameIndex)
        .addImm(0)
        .addMemOperand(MF.getMachineMemOperand(
            MachinePointerInfo::getFixedStack(MF, FrameIndex),
            MachineMemOperand::MOLoad, 1, MFI.getObjectAlign(FrameIndex)));
    return;
  }

  {
    const TargetRegisterInfo *TRI = STI->getRegisterInfo();
    if (RC->hasSuperClassEq(&Z80::GR16RegClass) ||
        TRI->getCommonSubClass(RC, &Z80::GR16RegClass) ||
        Z80::IR16RegClass.hasSubClassEq(RC)) {
      BuildMI(MBB, MI, DL, get(Z80::RELOAD_GR16))
          .addReg(DestReg, RegState::Define)
          .addFrameIndex(FrameIndex)
          .addImm(0)
          .addMemOperand(MF.getMachineMemOperand(
              MachinePointerInfo::getFixedStack(MF, FrameIndex),
              MachineMemOperand::MOLoad, 2, MFI.getObjectAlign(FrameIndex)));
      return;
    }
  }

  llvm_unreachable("loadRegFromStackSlot: unsupported register class");
}

/// Map a conditional branch opcode to its inverse.
/// Works with both JR and JP forms.
bool Z80InstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  assert(Cond.size() == 1 && "Invalid Z80 branch condition!");
  unsigned Opc = Cond[0].getImm();
  switch (Opc) {
  case Z80::JP_Z_nn:
    Cond[0].setImm(Z80::JP_NZ_nn);
    return false;
  case Z80::JP_NZ_nn:
    Cond[0].setImm(Z80::JP_Z_nn);
    return false;
  case Z80::JP_C_nn:
    Cond[0].setImm(Z80::JP_NC_nn);
    return false;
  case Z80::JP_NC_nn:
    Cond[0].setImm(Z80::JP_C_nn);
    return false;
  case Z80::JR_Z_e:
    Cond[0].setImm(Z80::JR_NZ_e);
    return false;
  case Z80::JR_NZ_e:
    Cond[0].setImm(Z80::JR_Z_e);
    return false;
  case Z80::JR_C_e:
    Cond[0].setImm(Z80::JR_NC_e);
    return false;
  case Z80::JR_NC_e:
    Cond[0].setImm(Z80::JR_C_e);
    return false;
  default:
    return true;
  }
}

int Z80InstrInfo::getSPAdjust(const MachineInstr &MI) const {
  unsigned Opc = MI.getOpcode();

  // ADJCALLSTACKDOWN is erased without physical SP change.
  // The actual SP adjustments come from individual PUSH instructions,
  // so we report 0 here to avoid double-counting.
  if (Opc == Z80::ADJCALLSTACKDOWN)
    return 0;

  // SPAdj uses the "offset correction" sign convention (StackGrowsDown):
  //   PUSH (SP decreases) → positive (offsets from SP need to increase)
  //   POP  (SP increases) → negative (offsets from SP need to decrease)
  // This matches the base class where ADJCALLSTACKDOWN returns positive.
  switch (Opc) {
  case Z80::PUSH_BC:
  case Z80::PUSH_DE:
  case Z80::PUSH_HL:
  case Z80::PUSH_AF:
  case Z80::PUSH_IX:
  case Z80::PUSH_IY:
    return 2;
  case Z80::POP_BC:
  case Z80::POP_DE:
  case Z80::POP_HL:
  case Z80::POP_AF:
  case Z80::POP_IX:
  case Z80::POP_IY:
    return -2;
  case Z80::INC_SP:
    return -1; // SP += 1 (pops 1 byte)
  case Z80::DEC_SP:
    return 1; // SP -= 1 (pushes 1 byte)
  default:
    return TargetInstrInfo::getSPAdjust(MI);
  }
}

bool Z80InstrInfo::expandPostRAPseudo(MachineInstr &MI) const {
  MachineBasicBlock &MBB = *MI.getParent();
  const TargetRegisterInfo *TRI = STI->getRegisterInfo();
  DebugLoc DL = MI.getDebugLoc();

  switch (MI.getOpcode()) {
  case Z80::LD_r8_n: {
    // LD_r8_n dst, imm -> LD_A_n/LD_B_n/etc. based on dst register
    Register DstReg = MI.getOperand(0).getReg();

    unsigned Opcode;
    switch (DstReg.id()) {
    case Z80::A:
      Opcode = Z80::LD_A_n;
      break;
    case Z80::B:
      Opcode = Z80::LD_B_n;
      break;
    case Z80::C:
      Opcode = Z80::LD_C_n;
      break;
    case Z80::D:
      Opcode = Z80::LD_D_n;
      break;
    case Z80::E:
      Opcode = Z80::LD_E_n;
      break;
    case Z80::H:
      Opcode = Z80::LD_H_n;
      break;
    case Z80::L:
      Opcode = Z80::LD_L_n;
      break;
    default:
      llvm_unreachable("Unexpected register for LD_r8_n");
    }

    BuildMI(MBB, MI, DL, get(Opcode)).add(MI.getOperand(1));
    MI.eraseFromParent();
    return true;
  }

  case Z80::LD_r16_nn: {
    // LD_r16_nn dst, imm -> LD_BC_nn/LD_DE_nn/LD_HL_nn based on dst register
    Register DstReg = MI.getOperand(0).getReg();

    unsigned Opcode;
    if (DstReg == Z80::BC)
      Opcode = Z80::LD_BC_nn;
    else if (DstReg == Z80::DE)
      Opcode = Z80::LD_DE_nn;
    else if (DstReg == Z80::HL)
      Opcode = Z80::LD_HL_nn;
    else
      llvm_unreachable("Unexpected register for LD_r16_nn");

    BuildMI(MBB, MI, DL, get(Opcode)).add(MI.getOperand(1));
    MI.eraseFromParent();
    return true;
  }

  case Z80::ZEXT_GR8_GR16: {
    // Zero extend 8-bit to 16-bit: LD lo,src; LD hi,0
    Register DstReg = MI.getOperand(0).getReg();
    Register SrcReg = MI.getOperand(1).getReg();
    Register LoReg = TRI->getSubReg(DstReg, Z80::sub_lo);
    Register HiReg = TRI->getSubReg(DstReg, Z80::sub_hi);
    if (!LoReg || !HiReg)
      return false;

    // Copy source to low byte (skip if already in place)
    if (SrcReg != LoReg) {
      unsigned CopyOp = Z80::getLD8RegOpcode(LoReg, SrcReg);
      if (!CopyOp)
        return false;
      BuildMI(MBB, MI, DL, get(CopyOp));
    }
    // Set high byte to 0
    unsigned ImmOp = getLoadImmR8Opcode(HiReg);
    if (!ImmOp)
      return false;
    BuildMI(MBB, MI, DL, get(ImmOp)).addImm(0);
    MI.eraseFromParent();
    return true;
  }

  case Z80::SEXT_GR8_GR16: {
    // Sign extend 8-bit to 16-bit:
    // LD A,src; LD lo,A; RLCA; SBC A,A; LD hi,A
    Register DstReg = MI.getOperand(0).getReg();
    Register SrcReg = MI.getOperand(1).getReg();
    Register LoReg = TRI->getSubReg(DstReg, Z80::sub_lo);
    Register HiReg = TRI->getSubReg(DstReg, Z80::sub_hi);
    if (!LoReg || !HiReg)
      return false;

    // Copy source to A (for sign-bit extraction)
    if (SrcReg != Z80::A) {
      unsigned CopyToA = Z80::getLD8RegOpcode(Z80::A, SrcReg);
      if (!CopyToA)
        return false;
      BuildMI(MBB, MI, DL, get(CopyToA));
    }
    // Copy A to low byte
    if (LoReg != Z80::A) {
      unsigned CopyToLo = Z80::getLD8RegOpcode(LoReg, Z80::A);
      if (!CopyToLo)
        return false;
      BuildMI(MBB, MI, DL, get(CopyToLo));
    }
    // RLCA rotates bit 7 into carry
    BuildMI(MBB, MI, DL, get(Z80::RLCA));
    // SBC A,A: A = 0xFF if carry (negative), 0x00 if not
    BuildMI(MBB, MI, DL, get(Z80::SBC_A_A));
    // Copy A (sign extension) to high byte
    if (HiReg != Z80::A) {
      unsigned CopyToHi = Z80::getLD8RegOpcode(HiReg, Z80::A);
      if (!CopyToHi)
        return false;
      BuildMI(MBB, MI, DL, get(CopyToHi));
    }
    MI.eraseFromParent();
    return true;
  }

  case Z80::SPILL_IMM8: {
    // SPILL_IMM8 val, offset -> LD (IX+d),n
    // Large offsets are handled in eliminateFrameIndex.
    int64_t Val = MI.getOperand(0).getImm();
    int64_t Offset = MI.getOperand(1).getImm();

    assert(Offset >= -128 && Offset <= 127 &&
           "Large offset should have been expanded in eliminateFrameIndex");
    BuildMI(MBB, MI, DL, get(Z80::LD_IXd_n)).addImm(Offset).addImm(Val & 0xFF);
    MI.eraseFromParent();
    return true;
  }

  case Z80::SPILL_GR8: {
    // SPILL_GR8 src, offset -> LD (IX+d),r
    // Large offsets are handled in eliminateFrameIndex.
    Register SrcReg = MI.getOperand(0).getReg();
    int64_t Offset = MI.getOperand(1).getImm();

    if (!SrcReg.isPhysical())
      return false;

    assert(Offset >= -128 && Offset <= 127 &&
           "Large offset should have been expanded in eliminateFrameIndex");
    unsigned Opcode = getStoreIXdOpcode(SrcReg);
    if (!Opcode)
      return false;
    BuildMI(MBB, MI, DL, get(Opcode)).addImm(Offset);
    MI.eraseFromParent();
    return true;
  }

  case Z80::RELOAD_GR8: {
    // RELOAD_GR8 dst, offset -> LD r,(IX+d)
    // Large offsets are handled in eliminateFrameIndex.
    Register DstReg = MI.getOperand(0).getReg();
    int64_t Offset = MI.getOperand(1).getImm();

    if (!DstReg.isPhysical())
      return false;

    assert(Offset >= -128 && Offset <= 127 &&
           "Large offset should have been expanded in eliminateFrameIndex");
    unsigned Opcode = getLoadIXdOpcode(DstReg);
    if (!Opcode)
      return false;
    BuildMI(MBB, MI, DL, get(Opcode)).addImm(Offset);
    MI.eraseFromParent();
    return true;
  }

  case Z80::SPILL_GR16: {
    // SPILL_GR16 src, offset -> LD (IX+d),lo ; LD (IX+d+1),hi
    // Large offsets are handled in eliminateFrameIndex.
    Register SrcReg = MI.getOperand(0).getReg();
    int64_t Offset = MI.getOperand(1).getImm();

    if (!SrcReg.isPhysical())
      return false;

    assert(Offset >= -128 && Offset + 1 <= 127 &&
           "Large offset should have been expanded in eliminateFrameIndex");

    // SP is not in GR16 register class, so it should never reach here.
    if (SrcReg == Z80::SP)
      llvm_unreachable("SP cannot be spilled via SPILL_GR16");

    Register LoReg = TRI->getSubReg(SrcReg, Z80::sub_lo);
    Register HiReg = TRI->getSubReg(SrcReg, Z80::sub_hi);
    if (!LoReg || !HiReg)
      return false;

    unsigned LoOp = getStoreIXdOpcode(LoReg);
    unsigned HiOp = getStoreIXdOpcode(HiReg);

    if (!LoOp || !HiOp) {
      if (SrcReg == Z80::IY) {
        // IY has no IX-indexed store opcodes, so transfer via HL.
        // Check if HL is live and save/restore it if needed.
        LivePhysRegs LiveRegs(*TRI);
        LiveRegs.addLiveOuts(MBB);
        for (auto I = MBB.rbegin(); &*I != &MI; ++I)
          LiveRegs.stepBackward(*I);
        bool NeedSaveHL =
            LiveRegs.contains(Z80::H) || LiveRegs.contains(Z80::L);
        if (NeedSaveHL)
          BuildMI(MBB, MI, DL, get(Z80::PUSH_HL));
        BuildMI(MBB, MI, DL, get(Z80::PUSH_IY));
        BuildMI(MBB, MI, DL, get(Z80::POP_HL));
        BuildMI(MBB, MI, DL, get(Z80::LD_IXd_L)).addImm(Offset);
        BuildMI(MBB, MI, DL, get(Z80::LD_IXd_H)).addImm(Offset + 1);
        if (NeedSaveHL)
          BuildMI(MBB, MI, DL, get(Z80::POP_HL));
        MI.eraseFromParent();
        return true;
      }
      return false;
    }

    BuildMI(MBB, MI, DL, get(LoOp)).addImm(Offset);
    BuildMI(MBB, MI, DL, get(HiOp)).addImm(Offset + 1);
    MI.eraseFromParent();
    return true;
  }

  case Z80::RELOAD_GR16: {
    // RELOAD_GR16 dst, offset -> LD lo,(IX+d) ; LD hi,(IX+d+1)
    // Large offsets are handled in eliminateFrameIndex.
    Register DestReg = MI.getOperand(0).getReg();
    int64_t Offset = MI.getOperand(1).getImm();

    if (!DestReg.isPhysical())
      return false;

    assert(Offset >= -128 && Offset + 1 <= 127 &&
           "Large offset should have been expanded in eliminateFrameIndex");

    // SP is not in GR16 register class, so it should never reach here.
    if (DestReg == Z80::SP)
      llvm_unreachable("SP cannot be reloaded via RELOAD_GR16");

    Register LoReg = TRI->getSubReg(DestReg, Z80::sub_lo);
    Register HiReg = TRI->getSubReg(DestReg, Z80::sub_hi);
    if (!LoReg || !HiReg)
      return false;

    unsigned LoOp = getLoadIXdOpcode(LoReg);
    unsigned HiOp = getLoadIXdOpcode(HiReg);

    if (!LoOp || !HiOp) {
      if (DestReg == Z80::IY) {
        // IY has no IX-indexed load opcodes, so transfer via HL.
        // Check if HL is live and save/restore it if needed.
        LivePhysRegs LiveRegs(*TRI);
        LiveRegs.addLiveOuts(MBB);
        for (auto I = MBB.rbegin(); &*I != &MI; ++I)
          LiveRegs.stepBackward(*I);
        bool NeedSaveHL =
            LiveRegs.contains(Z80::H) || LiveRegs.contains(Z80::L);
        if (NeedSaveHL)
          BuildMI(MBB, MI, DL, get(Z80::PUSH_HL));
        BuildMI(MBB, MI, DL, get(Z80::LD_L_IXd)).addImm(Offset);
        BuildMI(MBB, MI, DL, get(Z80::LD_H_IXd)).addImm(Offset + 1);
        BuildMI(MBB, MI, DL, get(Z80::PUSH_HL));
        BuildMI(MBB, MI, DL, get(Z80::POP_IY));
        if (NeedSaveHL)
          BuildMI(MBB, MI, DL, get(Z80::POP_HL));
        MI.eraseFromParent();
        return true;
      }
      return false;
    }

    BuildMI(MBB, MI, DL, get(LoOp)).addImm(Offset);
    BuildMI(MBB, MI, DL, get(HiOp)).addImm(Offset + 1);
    MI.eraseFromParent();
    return true;
  }

  case Z80::CMP16_FLAGS:
  case Z80::CMP16_ULT: {
    // 8-bit SUB/SBC chain for 16-bit unsigned comparison.
    // LD A,lhs_lo; SUB rhs_lo; LD A,lhs_hi; SBC A,rhs_hi
    // Does NOT clobber HL/DE. Only clobbers A and FLAGS.
    Register LHSReg = MI.getOperand(0).getReg();
    Register RHSReg = MI.getOperand(1).getReg();
    Register LhsLo = TRI->getSubReg(LHSReg, Z80::sub_lo);
    Register LhsHi = TRI->getSubReg(LHSReg, Z80::sub_hi);
    Register RhsLo = TRI->getSubReg(RHSReg, Z80::sub_lo);
    Register RhsHi = TRI->getSubReg(RHSReg, Z80::sub_hi);

    BuildMI(MBB, MI, DL, get(Z80::getLD8RegOpcode(Z80::A, LhsLo)));
    BuildMI(MBB, MI, DL, get(getSUBOpcode(RhsLo)));
    BuildMI(MBB, MI, DL, get(Z80::getLD8RegOpcode(Z80::A, LhsHi)));
    BuildMI(MBB, MI, DL, get(getSBCOpcode(RhsHi)));

    if (MI.getOpcode() == Z80::CMP16_ULT) {
      BuildMI(MBB, MI, DL, get(Z80::SBC_A_A));
      BuildMI(MBB, MI, DL, get(Z80::AND_n)).addImm(1);
    }

    MI.eraseFromParent();
    return true;
  }

  case Z80::CMP16_SBC_FLAGS: {
    // Carry-chain continuation: all SBC (no initial SUB).
    // LD A,lhs_lo; SBC A,rhs_lo; LD A,lhs_hi; SBC A,rhs_hi
    // Carry-in from previous CMP16_FLAGS or CMP16_SBC_FLAGS.
    Register LHSReg = MI.getOperand(0).getReg();
    Register RHSReg = MI.getOperand(1).getReg();
    Register LhsLo = TRI->getSubReg(LHSReg, Z80::sub_lo);
    Register LhsHi = TRI->getSubReg(LHSReg, Z80::sub_hi);
    Register RhsLo = TRI->getSubReg(RHSReg, Z80::sub_lo);
    Register RhsHi = TRI->getSubReg(RHSReg, Z80::sub_hi);

    BuildMI(MBB, MI, DL, get(Z80::getLD8RegOpcode(Z80::A, LhsLo)));
    BuildMI(MBB, MI, DL, get(getSBCOpcode(RhsLo)));
    BuildMI(MBB, MI, DL, get(Z80::getLD8RegOpcode(Z80::A, LhsHi)));
    BuildMI(MBB, MI, DL, get(getSBCOpcode(RhsHi)));

    MI.eraseFromParent();
    return true;
  }

  case Z80::XOR_CMP_EQ16:
  case Z80::XOR_CMP_NE16: {
    // XOR-based 16-bit equality comparison.
    // Compares two GR16 registers using byte-level XOR, produces 0/1 in A.
    // Does NOT clobber the source register pairs (unlike SBC HL,DE).
    // Only clobbers A and B.
    //
    // Sequence: LD A,lhs_hi; XOR rhs_hi; LD B,A; LD A,lhs_lo; XOR rhs_lo; OR B
    // Then normalize: EQ → SUB 1; SBC A,A; AND 1
    //                 NE → ADD 0xFF; SBC A,A; AND 1
    Register LHSReg = MI.getOperand(0).getReg();
    Register RHSReg = MI.getOperand(1).getReg();
    Register LHS_hi = TRI->getSubReg(LHSReg, Z80::sub_hi);
    Register LHS_lo = TRI->getSubReg(LHSReg, Z80::sub_lo);
    Register RHS_hi = TRI->getSubReg(RHSReg, Z80::sub_hi);
    Register RHS_lo = TRI->getSubReg(RHSReg, Z80::sub_lo);

    // XOR opcode table indexed by gr8RegToIndex
    static const unsigned XorOpcodes[] = {Z80::XOR_A, Z80::XOR_B, Z80::XOR_C,
                                          Z80::XOR_D, Z80::XOR_E, Z80::XOR_H,
                                          Z80::XOR_L};

    // XOR high bytes, save to B
    BuildMI(MBB, MI, DL, get(Z80::getLD8RegOpcode(Z80::A, LHS_hi)));
    BuildMI(MBB, MI, DL, get(XorOpcodes[Z80::gr8RegToIndex(RHS_hi)]));
    BuildMI(MBB, MI, DL, get(Z80::LD_B_A));
    // XOR low bytes, OR with saved high result
    BuildMI(MBB, MI, DL, get(Z80::getLD8RegOpcode(Z80::A, LHS_lo)));
    BuildMI(MBB, MI, DL, get(XorOpcodes[Z80::gr8RegToIndex(RHS_lo)]));
    BuildMI(MBB, MI, DL, get(Z80::OR_B));

    // Normalize to 0/1
    if (MI.getOpcode() == Z80::XOR_CMP_EQ16) {
      // A=0 (equal) → SUB 1 sets carry → SBC A,A → 0xFF → AND 1 → 1
      BuildMI(MBB, MI, DL, get(Z80::SUB_n)).addImm(1);
    } else {
      // A=0 (equal) → ADD 0xFF no carry → SBC A,A → 0 → AND 1 → 0
      BuildMI(MBB, MI, DL, get(Z80::ADD_A_n)).addImm(0xFF);
    }
    BuildMI(MBB, MI, DL, get(Z80::SBC_A_A));
    BuildMI(MBB, MI, DL, get(Z80::AND_n)).addImm(1);

    MI.eraseFromParent();
    return true;
  }

  case Z80::SM83_CMP_Z16:
  case Z80::XOR_CMP_Z16: {
    // 16-bit XOR-based equality comparison — sets Z flag directly.
    // Sequence: LD A,lhs_hi; XOR rhs_hi; LD B,A; LD A,lhs_lo; XOR rhs_lo; OR B
    // After OR B: Z=1 if equal, Z=0 if not equal.
    Register LHSReg = MI.getOperand(0).getReg();
    Register RHSReg = MI.getOperand(1).getReg();
    Register LHS_hi = TRI->getSubReg(LHSReg, Z80::sub_hi);
    Register LHS_lo = TRI->getSubReg(LHSReg, Z80::sub_lo);
    Register RHS_hi = TRI->getSubReg(RHSReg, Z80::sub_hi);
    Register RHS_lo = TRI->getSubReg(RHSReg, Z80::sub_lo);

    static const unsigned XorOpcodes[] = {Z80::XOR_A, Z80::XOR_B, Z80::XOR_C,
                                          Z80::XOR_D, Z80::XOR_E, Z80::XOR_H,
                                          Z80::XOR_L};

    BuildMI(MBB, MI, DL, get(Z80::getLD8RegOpcode(Z80::A, LHS_hi)));
    BuildMI(MBB, MI, DL, get(XorOpcodes[Z80::gr8RegToIndex(RHS_hi)]));
    BuildMI(MBB, MI, DL, get(Z80::LD_B_A));
    BuildMI(MBB, MI, DL, get(Z80::getLD8RegOpcode(Z80::A, LHS_lo)));
    BuildMI(MBB, MI, DL, get(XorOpcodes[Z80::gr8RegToIndex(RHS_lo)]));
    BuildMI(MBB, MI, DL, get(Z80::OR_B));

    MI.eraseFromParent();
    return true;
  }

  case Z80::ADD_HL_rr: {
    // ADD HL,rr — select ADD_HL_BC or ADD_HL_DE based on allocated register.
    Register RHS = MI.getOperand(0).getReg();
    unsigned AddOpc;
    if (RHS == Z80::BC)
      AddOpc = Z80::ADD_HL_BC;
    else if (RHS == Z80::DE)
      AddOpc = Z80::ADD_HL_DE;
    else
      llvm_unreachable("ADD_HL_rr: unexpected register");
    BuildMI(MBB, MI, DL, get(AddOpc));
    MI.eraseFromParent();
    return true;
  }

  case Z80::SUB_HL_rr: {
    // 16-bit subtraction: HL = HL - rr (no borrow in).
    Register RHS = MI.getOperand(0).getReg();
    if (STI->hasSM83()) {
      // SM83: byte-by-byte SUB/SBC (no 16-bit SBC HL,rr instruction).
      // LD A,L; SUB lo; LD L,A; LD A,H; SBC A,hi; LD H,A
      auto [Lo, Hi] = getSubRegs16(RHS);
      BuildMI(MBB, MI, DL, get(Z80::LD_A_L));
      BuildMI(MBB, MI, DL, get(getSUBOpcode(Lo)));
      BuildMI(MBB, MI, DL, get(Z80::LD_L_A));
      BuildMI(MBB, MI, DL, get(Z80::LD_A_H));
      BuildMI(MBB, MI, DL, get(getSBCOpcode(Hi)));
      BuildMI(MBB, MI, DL, get(Z80::LD_H_A));
    } else {
      // Z80: AND A; SBC HL,rr — atomic to prevent FLAGS clobbering.
      unsigned SbcOpc = (RHS == Z80::BC) ? Z80::SBC_HL_BC : Z80::SBC_HL_DE;
      BuildMI(MBB, MI, DL, get(Z80::AND_A));
      BuildMI(MBB, MI, DL, get(SbcOpc));
    }
    MI.eraseFromParent();
    return true;
  }

  case Z80::SADD_HL_rr: {
    // Signed 16-bit addition (sets P/V for overflow on Z80).
    Register RHS = MI.getOperand(0).getReg();
    if (STI->hasSM83()) {
      // SM83: byte-by-byte ADD/ADC (no ADC HL,rr; no P/V flag).
      // LD A,L; ADD A,lo; LD L,A; LD A,H; ADC A,hi; LD H,A
      auto [Lo, Hi] = getSubRegs16(RHS);
      BuildMI(MBB, MI, DL, get(Z80::LD_A_L));
      BuildMI(MBB, MI, DL, get(getADD8Opcode(Lo)));
      BuildMI(MBB, MI, DL, get(Z80::LD_L_A));
      BuildMI(MBB, MI, DL, get(Z80::LD_A_H));
      BuildMI(MBB, MI, DL, get(getADCOpcode(Hi)));
      BuildMI(MBB, MI, DL, get(Z80::LD_H_A));
    } else {
      // Z80: AND A; ADC HL,rr — sets P/V for overflow detection.
      unsigned AdcOpc = (RHS == Z80::BC) ? Z80::ADC_HL_BC : Z80::ADC_HL_DE;
      BuildMI(MBB, MI, DL, get(Z80::AND_A));
      BuildMI(MBB, MI, DL, get(AdcOpc));
    }
    MI.eraseFromParent();
    return true;
  }

  case Z80::ADD_HL_rr_CO: {
    // ADD HL,rr; SBC A,A; AND 1 — carry out in A.
    Register RHS = MI.getOperand(0).getReg();
    unsigned AddOpc;
    if (RHS == Z80::BC)
      AddOpc = Z80::ADD_HL_BC;
    else if (RHS == Z80::DE)
      AddOpc = Z80::ADD_HL_DE;
    else
      llvm_unreachable("ADD_HL_rr_CO: unexpected register");
    BuildMI(MBB, MI, DL, get(AddOpc));
    BuildMI(MBB, MI, DL, get(Z80::SBC_A_A));
    BuildMI(MBB, MI, DL, get(Z80::AND_n)).addImm(1);
    MI.eraseFromParent();
    return true;
  }

  case Z80::SUB_HL_rr_BO: {
    // 16-bit subtraction with borrow out: HL = HL - rr, A = borrow.
    Register RHS = MI.getOperand(0).getReg();
    if (STI->hasSM83()) {
      // SM83: byte-by-byte SUB/SBC + capture borrow.
      // LD A,L; SUB lo; LD L,A; LD A,H; SBC A,hi; LD H,A; SBC A,A; AND 1
      auto [Lo, Hi] = getSubRegs16(RHS);
      BuildMI(MBB, MI, DL, get(Z80::LD_A_L));
      BuildMI(MBB, MI, DL, get(getSUBOpcode(Lo)));
      BuildMI(MBB, MI, DL, get(Z80::LD_L_A));
      BuildMI(MBB, MI, DL, get(Z80::LD_A_H));
      BuildMI(MBB, MI, DL, get(getSBCOpcode(Hi)));
      BuildMI(MBB, MI, DL, get(Z80::LD_H_A));
    } else {
      // Z80: AND A; SBC HL,rr
      unsigned SbcOpc = (RHS == Z80::BC) ? Z80::SBC_HL_BC : Z80::SBC_HL_DE;
      BuildMI(MBB, MI, DL, get(Z80::AND_A));
      BuildMI(MBB, MI, DL, get(SbcOpc));
    }
    // Capture borrow out: SBC A,A; AND 1
    BuildMI(MBB, MI, DL, get(Z80::SBC_A_A));
    BuildMI(MBB, MI, DL, get(Z80::AND_n)).addImm(1);
    MI.eraseFromParent();
    return true;
  }

  case Z80::ADC_HL_rr_CIO: {
    // 16-bit add with carry in/out: HL = HL + rr + carry_in, A = carry_out.
    Register RHS = MI.getOperand(0).getReg();
    Register CarryReg = MI.getOperand(1).getReg();
    // Restore carry flag from carry_in register: LD A,carry; RRCA
    if (CarryReg != Z80::A) {
      unsigned LdOpc = Z80::getLD8RegOpcode(Z80::A, CarryReg);
      assert(LdOpc && "unexpected carry register for ADC_HL_rr_CIO");
      BuildMI(MBB, MI, DL, get(LdOpc));
    }
    BuildMI(MBB, MI, DL, get(Z80::RRCA));
    if (STI->hasSM83()) {
      // SM83: byte-by-byte ADC (carry flag set by RRCA above).
      // LD A,L; ADC A,lo; LD L,A; LD A,H; ADC A,hi; LD H,A
      auto [Lo, Hi] = getSubRegs16(RHS);
      BuildMI(MBB, MI, DL, get(Z80::LD_A_L));
      BuildMI(MBB, MI, DL, get(getADCOpcode(Lo)));
      BuildMI(MBB, MI, DL, get(Z80::LD_L_A));
      BuildMI(MBB, MI, DL, get(Z80::LD_A_H));
      BuildMI(MBB, MI, DL, get(getADCOpcode(Hi)));
      BuildMI(MBB, MI, DL, get(Z80::LD_H_A));
    } else {
      // Z80: ADC HL,rr (reads carry from RRCA above).
      unsigned AdcOpc = (RHS == Z80::BC) ? Z80::ADC_HL_BC : Z80::ADC_HL_DE;
      BuildMI(MBB, MI, DL, get(AdcOpc));
    }
    // Capture carry out: SBC A,A; AND 1
    BuildMI(MBB, MI, DL, get(Z80::SBC_A_A));
    BuildMI(MBB, MI, DL, get(Z80::AND_n)).addImm(1);
    MI.eraseFromParent();
    return true;
  }

  case Z80::SBC_HL_rr_BIO: {
    // 16-bit sub with borrow in/out: HL = HL - rr - borrow_in, A = borrow_out.
    Register RHS = MI.getOperand(0).getReg();
    Register BorrowReg = MI.getOperand(1).getReg();
    // Restore borrow flag from borrow_in register: LD A,borrow; RRCA
    if (BorrowReg != Z80::A) {
      unsigned LdOpc = Z80::getLD8RegOpcode(Z80::A, BorrowReg);
      assert(LdOpc && "unexpected borrow register for SBC_HL_rr_BIO");
      BuildMI(MBB, MI, DL, get(LdOpc));
    }
    BuildMI(MBB, MI, DL, get(Z80::RRCA));
    if (STI->hasSM83()) {
      // SM83: byte-by-byte SBC (borrow flag set by RRCA above).
      // LD A,L; SBC A,lo; LD L,A; LD A,H; SBC A,hi; LD H,A
      auto [Lo, Hi] = getSubRegs16(RHS);
      BuildMI(MBB, MI, DL, get(Z80::LD_A_L));
      BuildMI(MBB, MI, DL, get(getSBCOpcode(Lo)));
      BuildMI(MBB, MI, DL, get(Z80::LD_L_A));
      BuildMI(MBB, MI, DL, get(Z80::LD_A_H));
      BuildMI(MBB, MI, DL, get(getSBCOpcode(Hi)));
      BuildMI(MBB, MI, DL, get(Z80::LD_H_A));
    } else {
      // Z80: SBC HL,rr (reads borrow from RRCA above).
      unsigned SbcOpc = (RHS == Z80::BC) ? Z80::SBC_HL_BC : Z80::SBC_HL_DE;
      BuildMI(MBB, MI, DL, get(SbcOpc));
    }
    // Capture borrow out: SBC A,A; AND 1
    BuildMI(MBB, MI, DL, get(Z80::SBC_A_A));
    BuildMI(MBB, MI, DL, get(Z80::AND_n)).addImm(1);
    MI.eraseFromParent();
    return true;
  }

  case Z80::ADD_A_r:
  case Z80::SUB_r:
  case Z80::AND_r:
  case Z80::OR_r:
  case Z80::XOR_r:
  case Z80::CP_r: {
    // 8-bit ALU pseudo: select concrete opcode based on allocated register.
    Register RHS = MI.getOperand(0).getReg();
    static const unsigned AluOpcodes[][7] = {
        // A,       B,       C,       D,       E,       H,       L
        {Z80::ADD_A_A, Z80::ADD_A_B, Z80::ADD_A_C, Z80::ADD_A_D, Z80::ADD_A_E,
         Z80::ADD_A_H, Z80::ADD_A_L}, // ADD_A_r
        {Z80::SUB_A, Z80::SUB_B, Z80::SUB_C, Z80::SUB_D, Z80::SUB_E, Z80::SUB_H,
         Z80::SUB_L}, // SUB_r
        {Z80::AND_A, Z80::AND_B, Z80::AND_C, Z80::AND_D, Z80::AND_E, Z80::AND_H,
         Z80::AND_L}, // AND_r
        {Z80::OR_A, Z80::OR_B, Z80::OR_C, Z80::OR_D, Z80::OR_E, Z80::OR_H,
         Z80::OR_L}, // OR_r
        {Z80::XOR_A, Z80::XOR_B, Z80::XOR_C, Z80::XOR_D, Z80::XOR_E, Z80::XOR_H,
         Z80::XOR_L}, // XOR_r
        {Z80::CP_A, Z80::CP_B, Z80::CP_C, Z80::CP_D, Z80::CP_E, Z80::CP_H,
         Z80::CP_L}, // CP_r
    };
    unsigned TableIdx;
    switch (MI.getOpcode()) {
    case Z80::ADD_A_r:
      TableIdx = 0;
      break;
    case Z80::SUB_r:
      TableIdx = 1;
      break;
    case Z80::AND_r:
      TableIdx = 2;
      break;
    case Z80::OR_r:
      TableIdx = 3;
      break;
    case Z80::XOR_r:
      TableIdx = 4;
      break;
    case Z80::CP_r:
      TableIdx = 5;
      break;
    default:
      llvm_unreachable("unexpected 8-bit ALU pseudo");
    }
    int RegIdx = Z80::gr8RegToIndex(Register(RHS));
    assert(RegIdx >= 0 && "unexpected register for 8-bit ALU pseudo");
    BuildMI(MBB, MI, DL, get(AluOpcodes[TableIdx][RegIdx]));
    MI.eraseFromParent();
    return true;
  }

  case Z80::CAPTURE_PV: {
    // Read P/V flag (bit 2 of F register) into A as 0 or 1.
    // PUSH AF; POP HL; LD A,L; RRCA; RRCA; AND 1
    // PUSH/POP/LD don't affect flags, so P/V is preserved until RRCA.
    BuildMI(MBB, MI, DL, get(Z80::PUSH_AF));
    BuildMI(MBB, MI, DL, get(Z80::POP_HL));
    BuildMI(MBB, MI, DL, get(Z80::LD_A_L));
    BuildMI(MBB, MI, DL, get(Z80::RRCA));
    BuildMI(MBB, MI, DL, get(Z80::RRCA));
    BuildMI(MBB, MI, DL, get(Z80::AND_n)).addImm(1);
    MI.eraseFromParent();
    return true;
  }

  case Z80::SM83_SADDO_HL_rr: {
    // SM83 signed 16-bit add with overflow detection.
    // HL = HL + rr, A = overflow (0 or 1).
    // overflow = (result_hi ^ lhs_hi) & (result_hi ^ rhs_hi), bit 7
    Register RHS = MI.getOperand(0).getReg();
    auto [Lo, Hi] = getSubRegs16(RHS);
    // Use a temp from the "other" register pair.
    Register Temp = (RHS == Z80::DE) ? Z80::B : Z80::D;
    // Save lhs_hi before the addition overwrites H.
    BuildMI(MBB, MI, DL, get(Z80::getLD8RegOpcode(Temp, Z80::H)));
    // HL = HL + rr (byte-by-byte)
    BuildMI(MBB, MI, DL, get(Z80::LD_A_L));
    BuildMI(MBB, MI, DL, get(getADD8Opcode(Lo)));
    BuildMI(MBB, MI, DL, get(Z80::LD_L_A));
    BuildMI(MBB, MI, DL, get(Z80::LD_A_H));
    BuildMI(MBB, MI, DL, get(getADCOpcode(Hi)));
    BuildMI(MBB, MI, DL, get(Z80::LD_H_A));
    // A = result_hi. Compute overflow:
    // T1 = result_hi ^ lhs_hi (Temp has lhs_hi)
    unsigned XorTemp = (Temp == Z80::B) ? Z80::XOR_B : Z80::XOR_D;
    BuildMI(MBB, MI, DL, get(XorTemp)); // A = result_hi ^ lhs_hi
    BuildMI(MBB, MI, DL, get(Z80::getLD8RegOpcode(Temp, Z80::A))); // Temp = T1
    BuildMI(MBB, MI, DL, get(Z80::LD_A_H)); // A = result_hi
    // T2 = result_hi ^ rhs_hi
    unsigned XorHi = (Hi == Z80::B) ? Z80::XOR_B : Z80::XOR_D;
    BuildMI(MBB, MI, DL, get(XorHi)); // A = result_hi ^ rhs_hi
    // A = T1 & T2
    unsigned AndTemp = (Temp == Z80::B) ? Z80::AND_B : Z80::AND_D;
    BuildMI(MBB, MI, DL, get(AndTemp)); // A = (res^lhs) & (res^rhs)
    // Bit 7 → bit 0
    BuildMI(MBB, MI, DL, get(Z80::RLCA));
    BuildMI(MBB, MI, DL, get(Z80::AND_n)).addImm(1);
    MI.eraseFromParent();
    return true;
  }

  case Z80::SM83_SSUBO_HL_rr: {
    // SM83 signed 16-bit sub with overflow detection.
    // HL = HL - rr, A = overflow (0 or 1).
    // overflow = (result_hi ^ lhs_hi) & (lhs_hi ^ rhs_hi), bit 7
    Register RHS = MI.getOperand(0).getReg();
    auto [Lo, Hi] = getSubRegs16(RHS);
    // Use temps from the "other" register pair.
    Register Temp1 = (RHS == Z80::DE) ? Z80::B : Z80::D; // lhs_hi
    Register Temp2 = (RHS == Z80::DE) ? Z80::C : Z80::E; // XOR intermediate
    // Save lhs_hi before the subtraction overwrites H.
    BuildMI(MBB, MI, DL, get(Z80::getLD8RegOpcode(Temp1, Z80::H)));
    // HL = HL - rr (byte-by-byte)
    BuildMI(MBB, MI, DL, get(Z80::LD_A_L));
    BuildMI(MBB, MI, DL, get(getSUBOpcode(Lo)));
    BuildMI(MBB, MI, DL, get(Z80::LD_L_A));
    BuildMI(MBB, MI, DL, get(Z80::LD_A_H));
    BuildMI(MBB, MI, DL, get(getSBCOpcode(Hi)));
    BuildMI(MBB, MI, DL, get(Z80::LD_H_A));
    // A = result_hi. Compute overflow:
    // T1 = result_hi ^ lhs_hi (Temp1 has lhs_hi)
    unsigned XorTemp1 = (Temp1 == Z80::B) ? Z80::XOR_B : Z80::XOR_D;
    BuildMI(MBB, MI, DL, get(XorTemp1)); // A = result_hi ^ lhs_hi
    BuildMI(MBB, MI, DL,
            get(Z80::getLD8RegOpcode(Temp2, Z80::A))); // Temp2 = T1
    // T2 = lhs_hi ^ rhs_hi (need lhs_hi again, still in Temp1)
    BuildMI(MBB, MI, DL,
            get(Z80::getLD8RegOpcode(Z80::A, Temp1))); // A = lhs_hi
    unsigned XorHi = (Hi == Z80::B) ? Z80::XOR_B : Z80::XOR_D;
    BuildMI(MBB, MI, DL, get(XorHi)); // A = lhs_hi ^ rhs_hi
    // A = T1 & T2
    unsigned AndTemp2 = (Temp2 == Z80::C) ? Z80::AND_C : Z80::AND_E;
    BuildMI(MBB, MI, DL, get(AndTemp2)); // A = (res^lhs) & (lhs^rhs)
    // Bit 7 → bit 0
    BuildMI(MBB, MI, DL, get(Z80::RLCA));
    BuildMI(MBB, MI, DL, get(Z80::AND_n)).addImm(1);
    MI.eraseFromParent();
    return true;
  }

  case Z80::LSHR16:
  case Z80::ASHR16: {
    // 16-bit shift right by 1.
    // LSHR16: SRL hi; RR lo  (logical)
    // ASHR16: SRA hi; RR lo  (arithmetic, preserves sign)
    Register Reg = MI.getOperand(0).getReg();
    Register Hi = TRI->getSubReg(Reg, Z80::sub_hi);
    Register Lo = TRI->getSubReg(Reg, Z80::sub_lo);

    static const unsigned SrlOps[] = {Z80::SRL_A, Z80::SRL_B, Z80::SRL_C,
                                      Z80::SRL_D, Z80::SRL_E, Z80::SRL_H,
                                      Z80::SRL_L};
    static const unsigned SraOps[] = {Z80::SRA_A, Z80::SRA_B, Z80::SRA_C,
                                      Z80::SRA_D, Z80::SRA_E, Z80::SRA_H,
                                      Z80::SRA_L};
    static const unsigned RrOps[] = {Z80::RR_A, Z80::RR_B, Z80::RR_C, Z80::RR_D,
                                     Z80::RR_E, Z80::RR_H, Z80::RR_L};
    auto getSrlOp = [](Register R) { return SrlOps[Z80::gr8RegToIndex(R)]; };
    auto getSraOp = [](Register R) { return SraOps[Z80::gr8RegToIndex(R)]; };
    auto getRrOp = [](Register R) { return RrOps[Z80::gr8RegToIndex(R)]; };

    bool IsLogical = (MI.getOpcode() == Z80::LSHR16);
    BuildMI(MBB, MI, DL, get(IsLogical ? getSrlOp(Hi) : getSraOp(Hi)));
    BuildMI(MBB, MI, DL, get(getRrOp(Lo)));
    MI.eraseFromParent();
    return true;
  }

  case Z80::CALL_IY: {
    // Z80: Indirect call through IY register.
    // Expand to: CALL __call_iy (runtime trampoline that does JP (IY))
    MachineFunction &MF = *MBB.getParent();
    MCContext &Ctx = MF.getContext();
    MCSymbol *Sym = Ctx.getOrCreateSymbol("__call_iy");
    auto NewMI = BuildMI(MBB, MI, DL, get(Z80::CALL_nn)).addSym(Sym);
    // Copy implicit operands (argument registers)
    for (const MachineOperand &MO : MI.implicit_operands()) {
      if (MO.isReg() && MO.isUse())
        NewMI.addReg(MO.getReg(), RegState::Implicit);
    }
    MI.eraseFromParent();
    return true;
  }

  case Z80::CALL_HL: {
    // SM83: Indirect call through HL register.
    // Expand to: CALL __call_hl
    // The runtime trampoline is just JP (HL) — no register shift needed.
    MachineFunction &MF = *MBB.getParent();
    MCContext &Ctx = MF.getContext();
    MCSymbol *Sym = Ctx.getOrCreateSymbol("__call_hl");
    auto NewMI = BuildMI(MBB, MI, DL, get(Z80::CALL_nn)).addSym(Sym);
    for (const MachineOperand &MO : MI.implicit_operands()) {
      if (MO.isReg() && MO.isUse())
        NewMI.addReg(MO.getReg(), RegState::Implicit);
    }
    MI.eraseFromParent();
    return true;
  }

  case Z80::RET_CLEANUP: {
    // Callee-cleanup return: pop return address, skip N bytes of stack args,
    // then return via indirect jump.
    unsigned Amount = MI.getOperand(0).getImm();
    if (Amount == 0) {
      BuildMI(MBB, MI, DL, get(Z80::RET));
    } else if (STI->hasSM83()) {
      // SM83: POP HL; ADD SP,e; JP (HL)
      BuildMI(MBB, MI, DL, get(Z80::POP_HL));
      BuildMI(MBB, MI, DL, get(Z80::ADD_SP_e)).addImm(Amount & 0xFF);
      BuildMI(MBB, MI, DL, get(Z80::JP_HLind));
    } else if (Amount <= 8) {
      // Z80 small: POP BC; INC SP × N; PUSH BC; RET
      // Use BC (not HL) because HL may hold i32/float return high word.
      // BC is caller-saved and never part of any Z80 return value.
      BuildMI(MBB, MI, DL, get(Z80::POP_BC));
      for (unsigned i = 0; i < Amount; ++i)
        BuildMI(MBB, MI, DL, get(Z80::INC_SP));
      BuildMI(MBB, MI, DL, get(Z80::PUSH_BC));
      BuildMI(MBB, MI, DL, get(Z80::RET));
    } else {
      // Z80 large: POP BC; LD HL,N; ADD HL,SP; LD SP,HL; PUSH BC; RET
      // NOTE: This clobbers HL. Safe for __sdcccall(1) where Amount>8 only
      // occurs with RetBits<=16 (return in A/DE). For future __z88dk_callee
      // with i32/float returns (HLDE), this path needs revision.
      BuildMI(MBB, MI, DL, get(Z80::POP_BC));
      BuildMI(MBB, MI, DL, get(Z80::LD_HL_nn)).addImm(Amount);
      BuildMI(MBB, MI, DL, get(Z80::ADD_HL_SP));
      BuildMI(MBB, MI, DL, get(Z80::LD_SP_HL));
      BuildMI(MBB, MI, DL, get(Z80::PUSH_BC));
      BuildMI(MBB, MI, DL, get(Z80::RET));
    }
    MI.eraseFromParent();
    return true;
  }

  case Z80::SEXT16: {
    // Sign extension: 16-bit register → all sign bits (0x0000 or 0xFFFF)
    // LD A,src_hi; ADD A,A; SBC A,A; LD dst_lo,A; LD dst_hi,A
    Register DstReg = MI.getOperand(0).getReg();
    Register SrcReg = MI.getOperand(1).getReg();
    Register SrcHi = TRI->getSubReg(SrcReg, Z80::sub_hi);
    Register DstHi = TRI->getSubReg(DstReg, Z80::sub_hi);
    Register DstLo = TRI->getSubReg(DstReg, Z80::sub_lo);

    // LD A, src_hi - read the sign byte
    BuildMI(MBB, MI, DL, get(Z80::getLD8RegOpcode(Z80::A, SrcHi)));
    // ADD A,A - shift sign bit into carry
    BuildMI(MBB, MI, DL, get(Z80::ADD_A_A));
    // SBC A,A - A = 0xFF if carry (negative), 0x00 if no carry (positive)
    BuildMI(MBB, MI, DL, get(Z80::SBC_A_A));
    // LD dst_lo, A; LD dst_hi, A
    BuildMI(MBB, MI, DL, get(Z80::getLD8RegOpcode(DstLo, Z80::A)));
    BuildMI(MBB, MI, DL, get(Z80::getLD8RegOpcode(DstHi, Z80::A)));

    MI.eraseFromParent();
    return true;
  }

  case Z80::INC16:
  case Z80::DEC16: {
    // INC16/DEC16 pseudo: expand to the correct INC/DEC based on physical reg
    Register Reg = MI.getOperand(0).getReg();
    unsigned Opc;
    bool IsInc = (MI.getOpcode() == Z80::INC16);
    if (Reg == Z80::BC)
      Opc = IsInc ? Z80::INC_BC : Z80::DEC_BC;
    else if (Reg == Z80::DE)
      Opc = IsInc ? Z80::INC_DE : Z80::DEC_DE;
    else if (Reg == Z80::HL)
      Opc = IsInc ? Z80::INC_HL : Z80::DEC_HL;
    else if (Reg == Z80::IX)
      Opc = IsInc ? Z80::INC_IX : Z80::DEC_IX;
    else if (Reg == Z80::IY)
      Opc = IsInc ? Z80::INC_IY : Z80::DEC_IY;
    else
      return false;
    BuildMI(MBB, MI, DL, get(Opc));
    MI.eraseFromParent();
    return true;
  }

  default:
    return false;
  }
}

bool Z80InstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                 MachineBasicBlock *&TBB,
                                 MachineBasicBlock *&FBB,
                                 SmallVectorImpl<MachineOperand> &Cond,
                                 bool AllowModify) const {
  // Look at the last instructions of the block
  MachineBasicBlock::iterator I = MBB.end();
  while (I != MBB.begin()) {
    --I;

    if (I->isDebugInstr())
      continue;

    // Not a terminator - stop analyzing
    if (!I->isTerminator())
      break;

    // Handle unconditional branches
    if (I->getOpcode() == Z80::JP_nn || I->getOpcode() == Z80::JR_e) {
      if (TBB) {
        // Already have a target - this is the fallthrough case with conditional
        FBB = I->getOperand(0).getMBB();
      } else {
        TBB = I->getOperand(0).getMBB();
      }
      continue;
    }

    // Handle conditional branches
    if (I->getOpcode() == Z80::JP_Z_nn || I->getOpcode() == Z80::JP_NZ_nn ||
        I->getOpcode() == Z80::JP_C_nn || I->getOpcode() == Z80::JP_NC_nn ||
        I->getOpcode() == Z80::JR_Z_e || I->getOpcode() == Z80::JR_NZ_e ||
        I->getOpcode() == Z80::JR_C_e || I->getOpcode() == Z80::JR_NC_e) {
      if (!Cond.empty()) {
        // Already saw a conditional branch - can't analyze
        return true;
      }
      if (TBB) {
        // Unconditional branch was already seen (iterating backward).
        // Pattern: cond_br TrueTarget; jp FalseTarget
        // TBB currently holds FalseTarget from the unconditional JP.
        FBB = TBB;
      }
      TBB = I->getOperand(0).getMBB();
      Cond.push_back(MachineOperand::CreateImm(I->getOpcode()));
      continue;
    }

    // Unknown terminator
    return true;
  }

  return false;
}

unsigned Z80InstrInfo::insertBranch(MachineBasicBlock &MBB,
                                    MachineBasicBlock *TBB,
                                    MachineBasicBlock *FBB,
                                    ArrayRef<MachineOperand> Cond,
                                    const DebugLoc &DL, int *BytesAdded) const {
  assert(TBB && "insertBranch requires a target block");
  unsigned Count = 0;

  // Try to convert a conditional branch opcode to its JR form (2 bytes).
  // Returns the original JP opcode if no JR equivalent exists (P/M/PE/PO).
  auto toShortCond = [](unsigned Opc) -> unsigned {
    switch (Opc) {
    case Z80::JP_Z_nn:
    case Z80::JR_Z_e:
      return Z80::JR_Z_e;
    case Z80::JP_NZ_nn:
    case Z80::JR_NZ_e:
      return Z80::JR_NZ_e;
    case Z80::JP_C_nn:
    case Z80::JR_C_e:
      return Z80::JR_C_e;
    case Z80::JP_NC_nn:
    case Z80::JR_NC_e:
      return Z80::JR_NC_e;
    default:
      return Opc; // P/M/PE/PO — no JR form, keep JP
    }
  };

  if (Cond.empty()) {
    // Unconditional branch - emit JR (2 bytes), relaxed to JP if needed
    BuildMI(&MBB, DL, get(Z80::JR_e)).addMBB(TBB);
    ++Count;
    if (BytesAdded)
      *BytesAdded = 2;
  } else {
    // Conditional branch - emit JR cc (2 bytes) if possible, else JP cc (3)
    unsigned CondOpc = toShortCond(Cond[0].getImm());
    bool IsShort = CondOpc != Cond[0].getImm() || CondOpc == Z80::JR_Z_e ||
                   CondOpc == Z80::JR_NZ_e || CondOpc == Z80::JR_C_e ||
                   CondOpc == Z80::JR_NC_e;
    BuildMI(&MBB, DL, get(CondOpc)).addMBB(TBB);
    ++Count;
    if (BytesAdded)
      *BytesAdded = IsShort ? 2 : 3;

    if (FBB) {
      // Fallthrough branch
      BuildMI(&MBB, DL, get(Z80::JR_e)).addMBB(FBB);
      ++Count;
      if (BytesAdded)
        *BytesAdded += 2;
    }
  }

  return Count;
}

unsigned Z80InstrInfo::removeBranch(MachineBasicBlock &MBB,
                                    int *BytesRemoved) const {
  MachineBasicBlock::iterator I = MBB.end();
  unsigned Count = 0;
  int Removed = 0;

  while (I != MBB.begin()) {
    --I;

    if (I->isDebugInstr())
      continue;

    unsigned Opc = I->getOpcode();
    bool isJP = Opc == Z80::JP_nn || Opc == Z80::JP_Z_nn ||
                Opc == Z80::JP_NZ_nn || Opc == Z80::JP_C_nn ||
                Opc == Z80::JP_NC_nn;
    bool isJR = Opc == Z80::JR_e || Opc == Z80::JR_Z_e || Opc == Z80::JR_NZ_e ||
                Opc == Z80::JR_C_e || Opc == Z80::JR_NC_e;
    if (!isJP && !isJR)
      break;

    Removed += isJR ? 2 : 3;
    I->eraseFromParent();
    I = MBB.end();
    ++Count;
  }

  if (BytesRemoved)
    *BytesRemoved = Removed;

  return Count;
}

unsigned Z80InstrInfo::getInstSizeInBytes(const MachineInstr &MI) const {
  unsigned Opcode = MI.getOpcode();

  // ADD_HL_FI / SUB_HL_FI are expanded during eliminateFrameIndex (PEI),
  // which runs before BranchRelaxation. By the time branch distances matter,
  // these pseudos no longer exist. Return the FP-mode size (10B) as a
  // reasonable estimate for any pre-PEI pass that queries instruction size.
  // SP-relative mode expands to ~16B but is uncommon.
  if (Opcode == Z80::ADD_HL_FI || Opcode == Z80::SUB_HL_FI)
    return 10;

  // Inline-runtime pseudos expand to multiple MBBs after BranchRelaxation.
  // Report exact expanded sizes so BranchRelaxation can make precise decisions.
  // SM83 expansions are larger due to lacking ADC HL,HL / SBC HL,DE / EX DE,HL
  // / DJNZ and requiring byte-wise emulation + PUSH/POP AF for counter.
  {
    const auto &STI =
        MI.getParent()->getParent()->getSubtarget<Z80Subtarget>();
    bool IsSM83 = STI.hasSM83();
    switch (Opcode) {
    case Z80::MUL16:  return IsSM83 ? 24 : 20;
    case Z80::UDIV16: return IsSM83 ? 45 : 32;
    case Z80::UMOD16: return IsSM83 ? 45 : 31;
    case Z80::SDIV16: return IsSM83 ? 79 : 66;
    case Z80::SMOD16: return IsSM83 ? 78 : 64;
    default: break;
    }
  }

  // Handle pseudo-instructions that have no encoding
  if (MI.isDebugInstr() || MI.isLabel() || MI.isPseudo())
    return 0;

  // COPY is a pseudo that gets eliminated
  if (Opcode == TargetOpcode::COPY)
    return 0;

  // PHI nodes get eliminated
  if (Opcode == TargetOpcode::PHI)
    return 0;

  // Get the MCInstrDesc for size info
  const MCInstrDesc &Desc = MI.getDesc();

  // If the instruction has a fixed size from TableGen, use it
  unsigned Size = Desc.getSize();
  if (Size != 0)
    return Size;

  // Default sizes for Z80 instructions
  switch (Opcode) {
  // Single-byte instructions (most 8-bit ops, register-to-register)
  case Z80::NOP:
  case Z80::HALT:
  case Z80::RET:
  case Z80::LD_A_A:
  case Z80::LD_A_B:
  case Z80::LD_A_C:
  case Z80::LD_A_D:
  case Z80::LD_A_E:
  case Z80::LD_A_H:
  case Z80::LD_A_L:
  case Z80::LD_B_A:
  case Z80::LD_B_B:
  case Z80::LD_B_C:
  case Z80::LD_B_D:
  case Z80::LD_B_E:
  case Z80::LD_B_H:
  case Z80::LD_B_L:
  case Z80::LD_C_A:
  case Z80::LD_C_B:
  case Z80::LD_C_C:
  case Z80::LD_C_D:
  case Z80::LD_C_E:
  case Z80::LD_C_H:
  case Z80::LD_C_L:
  case Z80::LD_D_A:
  case Z80::LD_D_B:
  case Z80::LD_D_C:
  case Z80::LD_D_D:
  case Z80::LD_D_E:
  case Z80::LD_D_H:
  case Z80::LD_D_L:
  case Z80::LD_E_A:
  case Z80::LD_E_B:
  case Z80::LD_E_C:
  case Z80::LD_E_D:
  case Z80::LD_E_E:
  case Z80::LD_E_H:
  case Z80::LD_E_L:
  case Z80::LD_H_A:
  case Z80::LD_H_B:
  case Z80::LD_H_C:
  case Z80::LD_H_D:
  case Z80::LD_H_E:
  case Z80::LD_H_H:
  case Z80::LD_H_L:
  case Z80::LD_L_A:
  case Z80::LD_L_B:
  case Z80::LD_L_C:
  case Z80::LD_L_D:
  case Z80::LD_L_E:
  case Z80::LD_L_H:
  case Z80::LD_L_L:
  case Z80::LD_A_HLind:
  case Z80::LD_HLind_A:
  case Z80::ADD_A_A:
  case Z80::ADD_A_B:
  case Z80::ADD_A_C:
  case Z80::ADD_A_D:
  case Z80::ADD_A_E:
  case Z80::ADD_A_H:
  case Z80::ADD_A_L:
  case Z80::SUB_A:
  case Z80::SUB_B:
  case Z80::SUB_C:
  case Z80::SUB_D:
  case Z80::SUB_E:
  case Z80::SUB_H:
  case Z80::SUB_L:
  case Z80::AND_A:
  case Z80::AND_B:
  case Z80::AND_C:
  case Z80::AND_D:
  case Z80::AND_E:
  case Z80::AND_H:
  case Z80::AND_L:
  case Z80::OR_A:
  case Z80::OR_B:
  case Z80::OR_C:
  case Z80::OR_D:
  case Z80::OR_E:
  case Z80::OR_H:
  case Z80::OR_L:
  case Z80::XOR_A:
  case Z80::XOR_B:
  case Z80::XOR_C:
  case Z80::XOR_D:
  case Z80::XOR_E:
  case Z80::XOR_H:
  case Z80::XOR_L:
  case Z80::CP_A:
  case Z80::CP_B:
  case Z80::CP_C:
  case Z80::CP_D:
  case Z80::CP_E:
  case Z80::CP_H:
  case Z80::CP_L:
  case Z80::INC_A:
  case Z80::INC_B:
  case Z80::INC_C:
  case Z80::INC_D:
  case Z80::INC_E:
  case Z80::INC_H:
  case Z80::INC_L:
  case Z80::DEC_A:
  case Z80::DEC_B:
  case Z80::DEC_C:
  case Z80::DEC_D:
  case Z80::DEC_E:
  case Z80::DEC_H:
  case Z80::DEC_L:
  case Z80::INC_BC:
  case Z80::INC_DE:
  case Z80::INC_HL:
  case Z80::INC_SP:
  case Z80::DEC_BC:
  case Z80::DEC_DE:
  case Z80::DEC_HL:
  case Z80::DEC_SP:
  case Z80::ADD_HL_BC:
  case Z80::ADD_HL_DE:
  case Z80::ADD_HL_HL:
  case Z80::ADD_HL_SP:
  case Z80::ADD_HL_rr: // pseudo → 1-byte ADD HL,rr
  case Z80::ADD_A_r:
  case Z80::SUB_r:
  case Z80::AND_r:
  case Z80::OR_r:
  case Z80::XOR_r:
  case Z80::CP_r: // 8-bit ALU pseudos → 1 byte
  case Z80::PUSH_AF:
  case Z80::PUSH_BC:
  case Z80::PUSH_DE:
  case Z80::PUSH_HL:
  case Z80::POP_AF:
  case Z80::POP_BC:
  case Z80::POP_DE:
  case Z80::POP_HL:
  case Z80::RLCA:
  case Z80::RRCA:
  case Z80::RLA:
  case Z80::RRA:
  case Z80::SCF:
  case Z80::CCF:
  case Z80::SBC_A_A:
    return 1;

  // Two-byte instructions (with immediate or CB prefix)
  case Z80::LD_A_n:
  case Z80::LD_B_n:
  case Z80::LD_C_n:
  case Z80::LD_D_n:
  case Z80::LD_E_n:
  case Z80::LD_H_n:
  case Z80::LD_L_n:
  case Z80::ADD_A_n:
  case Z80::SUB_n:
  case Z80::AND_n:
  case Z80::OR_n:
  case Z80::XOR_n:
  case Z80::CP_n:
  case Z80::SLA_A:
  case Z80::SLA_B:
  case Z80::SLA_C:
  case Z80::SLA_D:
  case Z80::SLA_E:
  case Z80::SLA_H:
  case Z80::SLA_L:
  case Z80::SRA_A:
  case Z80::SRA_B:
  case Z80::SRA_C:
  case Z80::SRA_D:
  case Z80::SRA_E:
  case Z80::SRA_H:
  case Z80::SRA_L:
  case Z80::SRL_A:
  case Z80::SRL_B:
  case Z80::SRL_C:
  case Z80::SRL_D:
  case Z80::SRL_E:
  case Z80::SRL_H:
  case Z80::SRL_L:
  case Z80::SBC_HL_BC:
  case Z80::SBC_HL_DE:
  case Z80::SBC_HL_HL:
  case Z80::SBC_HL_SP:
  case Z80::ADC_HL_BC:
  case Z80::ADC_HL_DE:
  case Z80::ADC_HL_HL:
  case Z80::ADC_HL_SP:
  case Z80::PUSH_IX:
  case Z80::PUSH_IY:
  case Z80::POP_IX:
  case Z80::POP_IY:
    return 2;

  // Three-byte instructions (JP nn, CALL nn, LD rr,nn)
  // Also SUB_HL_xx pseudos (AND A=1 + SBC HL,xx=2 = 3 bytes)
  case Z80::JP_nn:
  case Z80::JP_Z_nn:
  case Z80::JP_NZ_nn:
  case Z80::JP_C_nn:
  case Z80::JP_NC_nn:
  case Z80::CALL_nn:
  case Z80::LD_BC_nn:
  case Z80::LD_DE_nn:
  case Z80::LD_HL_nn:
  case Z80::LD_SP_nn:
  case Z80::SUB_HL_rr:  // AND A(1) + SBC HL,rr(2) = 3
  case Z80::SADD_HL_rr: // AND A(1) + ADC HL,rr(2) = 3
    return 3;

  case Z80::CMP16_FLAGS: // LD A,lo(1) + SUB lo(1) + LD A,hi(1) + SBC A,hi(1) =
                         // 4
  case Z80::CMP16_SBC_FLAGS: // LD A,lo(1) + SBC A,lo(1) + LD A,hi(1) + SBC
                             // A,hi(1) = 4
    return 4;

  case Z80::ADD_HL_rr_CO: // ADD HL,rr(1) + SBC A,A(1) + AND n(2) = 4
    return 4;

  case Z80::SUB_HL_rr_BO: // AND A(1) + SBC HL,rr(2) + SBC A,A(1) + AND n(2) = 6
    return 6;

  case Z80::CMP16_ULT: // LD A,lo(1) + SUB lo(1) + LD A,hi(1) + SBC A,hi(1) +
                       // SBC A,A(1) + AND 1(2) = 7
    return 7;

  // CAPTURE_PV: PUSH AF(1) + POP HL(1) + LD A,L(1) + RRCA(1) + RRCA(1) + AND
  // n(2) = 7
  case Z80::CAPTURE_PV:
  case Z80::ADC_HL_rr_CIO: // LD A,r(1) + RRCA(1) + ADC HL,rr(2) + SBC A,A(1) +
                           // AND n(2) = 7
  case Z80::SBC_HL_rr_BIO: // LD A,r(1) + RRCA(1) + SBC HL,rr(2) + SBC A,A(1) +
                           // AND n(2) = 7
    return 7;

  // XOR-based comparison pseudos
  case Z80::SM83_CMP_Z16: // LD+XOR+LD B,A+LD+XOR+OR B = 6
  case Z80::XOR_CMP_Z16:
    return 6;
  case Z80::XOR_CMP_EQ16: // 6 (XOR chain) + SUB 1(2) + SBC A,A(1) + AND 1(2) =
                          // 11
  case Z80::XOR_CMP_NE16:
    return 11;

  // SM83 signed overflow pseudos: 12 x 1-byte + 1 x 2-byte = 14 bytes
  case Z80::SM83_SADDO_HL_rr:
  case Z80::SM83_SSUBO_HL_rr:
    return 14;

  // Four-byte instructions (IX/IY indexed)
  case Z80::LD_IX_nn:
  case Z80::LD_IY_nn:
    return 4;

  default:
    // For unknown instructions, return a safe default
    return 3;
  }
}

bool Z80InstrInfo::isBranchOffsetInRange(unsigned BranchOpc,
                                         int64_t BrOffset) const {
  switch (BranchOpc) {
  case Z80::JP_nn:
  case Z80::JP_Z_nn:
  case Z80::JP_NZ_nn:
  case Z80::JP_C_nn:
  case Z80::JP_NC_nn:
    return true;
  case Z80::JR_e:
  case Z80::JR_Z_e:
  case Z80::JR_NZ_e:
  case Z80::JR_C_e:
  case Z80::JR_NC_e:
    // JR uses a signed 8-bit offset from PC after the 2-byte instruction.
    // BrOffset is from the start of the instruction, so adjust by +2.
    return (BrOffset - 2) >= -128 && (BrOffset - 2) <= 127;
  default:
    return false;
  }
}

MachineBasicBlock *
Z80InstrInfo::getBranchDestBlock(const MachineInstr &MI) const {
  switch (MI.getOpcode()) {
  case Z80::JP_nn:
  case Z80::JP_Z_nn:
  case Z80::JP_NZ_nn:
  case Z80::JP_C_nn:
  case Z80::JP_NC_nn:
  case Z80::JR_e:
  case Z80::JR_Z_e:
  case Z80::JR_NZ_e:
  case Z80::JR_C_e:
  case Z80::JR_NC_e:
    return MI.getOperand(0).getMBB();
  default:
    llvm_unreachable("unexpected opcode in getBranchDestBlock");
  }
}

void Z80InstrInfo::insertIndirectBranch(MachineBasicBlock &MBB,
                                        MachineBasicBlock &NewDestBB,
                                        MachineBasicBlock &RestoreBB,
                                        const DebugLoc &DL, int64_t BrOffset,
                                        RegScavenger *RS) const {
  // On Z80, JP nn can reach any address in the 64KB space.
  BuildMI(&MBB, DL, get(Z80::JP_nn)).addMBB(&NewDestBB);
}
