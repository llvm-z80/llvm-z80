//===-- Z80RegisterInfo.cpp - Z80 Register Information --------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the Z80 implementation of the TargetRegisterInfo class.
//
//===----------------------------------------------------------------------===//

#include "Z80RegisterInfo.h"

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80.h"
#include "Z80FrameLowering.h"
#include "Z80InstrInfo.h"
#include "Z80OpcodeUtils.h"
#include "Z80Subtarget.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/Support/ErrorHandling.h"

#define DEBUG_TYPE "z80-reginfo"

#define GET_REGINFO_TARGET_DESC
#include "Z80GenRegisterInfo.inc"

using namespace llvm;

// Convenience aliases for shared opcode utilities.
static unsigned getStoreHLindOpcode(Register R) {
  return Z80::getStoreHLindOpcode(R);
}
static unsigned getLoadHLindOpcode(Register R) {
  return Z80::getLoadHLindOpcode(R);
}
static unsigned getCopyToAOpcode(Register R) {
  return Z80::getLD8RegOpcode(Z80::A, R);
}
static unsigned getCopyFromAOpcode(Register R) {
  return Z80::getLD8RegOpcode(R, Z80::A);
}

// Check if an 8-bit register is a sub-register of a 16-bit pair.
static bool isSubRegOf(Register Reg8, Register Reg16) {
  if (Reg16 == Z80::BC)
    return Reg8 == Z80::B || Reg8 == Z80::C;
  if (Reg16 == Z80::DE)
    return Reg8 == Z80::D || Reg8 == Z80::E;
  if (Reg16 == Z80::HL)
    return Reg8 == Z80::H || Reg8 == Z80::L;
  return false;
}

// Forward declaration — defined below in the liveness analysis section.
static bool isRegLiveAt(Register Reg, MachineBasicBlock &MBB,
                        MachineBasicBlock::iterator MI,
                        const TargetRegisterInfo *TRI);

// Choose a temp register from {BC, DE} that doesn't overlap with the given
// 8-bit register. Prefers a register that is not live to avoid save/restore.
static Register chooseTempReg(Register Avoid8, MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator MI,
                              const TargetRegisterInfo *TRI) {
  if (isSubRegOf(Avoid8, Z80::BC))
    return Z80::DE;
  if (isSubRegOf(Avoid8, Z80::DE))
    return Z80::BC;
  // Neither conflicts — pick a dead one if possible.
  auto NextIt = std::next(MI);
  if (!isRegLiveAt(Z80::BC, MBB, NextIt, TRI))
    return Z80::BC;
  if (!isRegLiveAt(Z80::DE, MBB, NextIt, TRI))
    return Z80::DE;
  return Z80::BC;
}

static unsigned getPushOpcode(Register R) { return Z80::getPushOpcode(R); }
static unsigned getPopOpcode(Register R) { return Z80::getPopOpcode(R); }

//===----------------------------------------------------------------------===//
// Z80RegisterInfo implementation
//===----------------------------------------------------------------------===//

Z80RegisterInfo::Z80RegisterInfo()
    : Z80GenRegisterInfo(/*RA=*/0, /*DwarfFlavor=*/0, /*EHFlavor=*/0,
                         /*PC=*/0, /*HwMode=*/0) {}

const MCPhysReg *
Z80RegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  const auto &STI = MF->getSubtarget<Z80Subtarget>();
  if (MF->getFunction().hasFnAttribute("interrupt")) {
    if (STI.hasSM83())
      return SM83_Interrupt_CSR_SaveList;
    return Z80_Interrupt_CSR_SaveList;
  }
  if (STI.hasSM83())
    return SM83_CSR_SaveList;
  return Z80_CSR_SaveList;
}

const uint32_t *
Z80RegisterInfo::getCallPreservedMask(const MachineFunction &MF,
                                      CallingConv::ID CallingConv) const {
  const auto &STI = MF.getSubtarget<Z80Subtarget>();
  if (STI.hasSM83())
    return SM83_CSR_RegMask;
  return Z80_CSR_RegMask;
}

BitVector Z80RegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs());
  const auto &STI = MF.getSubtarget<Z80Subtarget>();

  // Reserve the stack pointer
  Reserved.set(Z80::SP);

  // Reserve FLAGS: non-allocatable status register for dependency tracking
  Reserved.set(Z80::FLAGS);

  // Always reserve IX and IY (16-bit).
  // IX is the frame pointer when hasFP, and stack frame code uses it
  // unconditionally even without FP.
  // IY is reserved because spilling IY requires HL as a temporary.
  Reserved.set(Z80::IX);
  Reserved.set(Z80::IY);

  // IXH/IXL/IYH/IYL are undocumented Z80 half-index registers.
  // Only available when FeatureUndocumented is enabled; absent on SM83.
  if (!STI.hasUndocumented() || STI.hasSM83()) {
    Reserved.set(Z80::IXH);
    Reserved.set(Z80::IXL);
    Reserved.set(Z80::IYH);
    Reserved.set(Z80::IYL);
  }

  if (STI.hasSM83()) {
    // SM83 has no I/R or shadow registers.
    Reserved.set(Z80::I);
    Reserved.set(Z80::R);
    Reserved.set(Z80::AFp);
    Reserved.set(Z80::BCp);
    Reserved.set(Z80::DEp);
    Reserved.set(Z80::HLp);
    Reserved.set(Z80::Ap);
    Reserved.set(Z80::Fp);
    Reserved.set(Z80::Bp);
    Reserved.set(Z80::Cp);
    Reserved.set(Z80::Dp);
    Reserved.set(Z80::Ep);
    Reserved.set(Z80::Hp);
    Reserved.set(Z80::Lp);
  }

  return Reserved;
}

const TargetRegisterClass *
Z80RegisterInfo::getLargestLegalSuperClass(const TargetRegisterClass *RC,
                                           const MachineFunction &) const {
  if (RC->hasSuperClass(&Z80::Anyi8RegClass))
    return &Z80::Anyi8RegClass;
  if (RC->hasSuperClass(&Z80::Anyi16RegClass))
    return &Z80::Anyi16RegClass;
  return RC;
}

bool Z80RegisterInfo::saveScavengerRegister(MachineBasicBlock &MBB,
                                            MachineBasicBlock::iterator I,
                                            MachineBasicBlock::iterator &UseMI,
                                            const TargetRegisterClass *RC,
                                            Register Reg) const {
  const TargetInstrInfo &TII = *MBB.getParent()->getSubtarget().getInstrInfo();
  DebugLoc DL;

  // Z80 can only PUSH/POP 16-bit register pairs.
  // For 8-bit registers, save the containing pair.
  unsigned PushOpc, PopOpc;
  if (Reg == Z80::BC || Reg == Z80::B || Reg == Z80::C) {
    PushOpc = Z80::PUSH_BC;
    PopOpc = Z80::POP_BC;
  } else if (Reg == Z80::DE || Reg == Z80::D || Reg == Z80::E) {
    PushOpc = Z80::PUSH_DE;
    PopOpc = Z80::POP_DE;
  } else if (Reg == Z80::HL || Reg == Z80::H || Reg == Z80::L) {
    PushOpc = Z80::PUSH_HL;
    PopOpc = Z80::POP_HL;
  } else if (Reg == Z80::AF || Reg == Z80::A) {
    PushOpc = Z80::PUSH_AF;
    PopOpc = Z80::POP_AF;
  } else {
    return false;
  }

  BuildMI(MBB, I, DL, TII.get(PushOpc));
  BuildMI(MBB, std::next(UseMI), DL, TII.get(PopOpc));
  return true;
}

//===----------------------------------------------------------------------===//
// FLAGS liveness helper
//===----------------------------------------------------------------------===//

// Check if the FLAGS register is live after MI.
// Scans forward: if an instruction reads FLAGS before any instruction writes
// FLAGS, then FLAGS is live. Conditional branches (JP cc, JR cc) are
// terminators with Uses = [FLAGS], so they are caught by readsRegister.
// Also checks successor blocks for FLAGS live-in.
static bool isFlagsLiveAfter(MachineBasicBlock::iterator MI,
                             const TargetRegisterInfo *TRI) {
  MachineBasicBlock &MBB = *MI->getParent();
  for (auto I = std::next(MI->getIterator()); I != MBB.end(); ++I) {
    if (I->readsRegister(Z80::FLAGS, TRI))
      return true;
    if (I->modifiesRegister(Z80::FLAGS, TRI))
      return false;
  }
  // Reached end of block — check if FLAGS is live-in to any successor.
  for (const MachineBasicBlock *Succ : MBB.successors()) {
    if (Succ->isLiveIn(Z80::FLAGS))
      return true;
  }
  return false;
}

//===----------------------------------------------------------------------===//
// Address computation helpers
//===----------------------------------------------------------------------===//

// Emit: PUSH IX; POP HL; LD <TempReg>,offset; ADD HL,<TempReg>
// After this, HL = IX + offset. TempReg must be BC or DE.
// If PreserveFlags is true, wraps ADD HL with PUSH AF/POP AF to preserve
// the FLAGS register. This is safe because no instruction between PUSH AF
// and POP AF modifies A (PUSH IX, POP HL, LD rr,nn don't touch A).
static void emitLargeOffsetAddr(MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator InsertBefore,
                                const DebugLoc &DL, const TargetInstrInfo &TII,
                                int64_t Offset, Register TempReg,
                                bool PreserveFlags) {
  assert((TempReg == Z80::BC || TempReg == Z80::DE) &&
         "Address computation temp must be BC or DE");

  BuildMI(MBB, InsertBefore, DL, TII.get(Z80::PUSH_IX));
  BuildMI(MBB, InsertBefore, DL, TII.get(Z80::POP_HL));

  unsigned LdOpc = (TempReg == Z80::BC) ? Z80::LD_BC_nn : Z80::LD_DE_nn;
  unsigned AddOpc = (TempReg == Z80::BC) ? Z80::ADD_HL_BC : Z80::ADD_HL_DE;

  BuildMI(MBB, InsertBefore, DL, TII.get(LdOpc)).addImm(Offset & 0xFFFF);
  if (PreserveFlags)
    BuildMI(MBB, InsertBefore, DL, TII.get(Z80::PUSH_AF));
  BuildMI(MBB, InsertBefore, DL, TII.get(AddOpc));
  if (PreserveFlags)
    BuildMI(MBB, InsertBefore, DL, TII.get(Z80::POP_AF));
}

//===----------------------------------------------------------------------===//
// Register liveness analysis for frame index elimination
//===----------------------------------------------------------------------===//

// Check if Reg (or any overlapping sub/super-register) is live at MI by
// scanning forward through instructions. Replaces RegScavenger which is
// unreliable with forward frame index elimination (RS is not initialized
// per-BB in forward walk mode).
//
// We scan forward from MI until we either find:
// - An instruction that USES the register → it's live
// - An instruction that fully defines Reg (no use) → it's dead
// - The end of the basic block → check if live-out via successor live-ins
//
// Important: a partial def (e.g., defining H when querying HL) does NOT
// kill the entire register — the other half (L) may still be live.
// Only a def that fully covers Reg (Reg == DefReg, or Reg is a sub-register
// of DefReg) counts as a kill.
// Check whether ADJCALLSTACKUP will actually clobber Reg when expanded.
//
// ADJCALLSTACKUP carries implicit-def annotations for HL, A, SP, but the
// actual register side-effects depend on the expansion path chosen in
// Z80FrameLowering::eliminateCallFramePseudoInstr (which runs AFTER frame
// index elimination within PEI). The expansion paths are:
//
//   AdjAmount == 0         → erased entirely (no register effects)
//   SM83 && AdjAmount≤127  → ADD SP,e     (only SP/flags modified)
//   AdjAmount ≤ 16         → POP AF × N   (A/flags modified, HL untouched)
//   AdjAmount > 16         → LD HL,n; ADD HL,SP; LD SP,HL (HL/flags modified)
//
// If we naively trust the implicit-defs, isRegLiveAt() may conclude a
// register (e.g. HL) is dead when the pseudo won't actually modify it,
// causing SPILL/RELOAD expansion to skip saving the register.
static bool adjCallStackUpClobbersReg(const MachineInstr &MI, Register Reg,
                                      const TargetRegisterInfo *TRI) {
  assert(MI.getOpcode() == Z80::ADJCALLSTACKUP);
  int64_t AdjAmount = MI.getOperand(0).getImm() - MI.getOperand(1).getImm();

  if (AdjAmount == 0)
    return false;

  const auto &STI = MI.getMF()->getSubtarget<Z80Subtarget>();
  if (STI.hasSM83() && AdjAmount <= 127)
    // ADD SP,e: only SP and flags modified.
    return false;

  if (AdjAmount <= 16)
    // POP AF: clobbers A and flags; HL is untouched.
    return TRI->regsOverlap(Reg, Z80::A);

  // LD HL,n; ADD HL,SP; LD SP,HL: clobbers HL and flags.
  return TRI->regsOverlap(Reg, Z80::HL);
}

static bool isRegLiveAt(Register Reg, MachineBasicBlock &MBB,
                        MachineBasicBlock::iterator MI,
                        const TargetRegisterInfo *TRI) {
  for (auto I = MI, E = MBB.end(); I != E; ++I) {
    // ADJCALLSTACKDOWN is always erased without emitting any code.
    if (I->getOpcode() == Z80::ADJCALLSTACKDOWN)
      continue;

    // ADJCALLSTACKUP's implicit-defs don't always reflect reality —
    // the actual register clobbers depend on the expansion path.
    // Skip this pseudo if its expansion won't touch our register.
    if (I->getOpcode() == Z80::ADJCALLSTACKUP &&
        !adjCallStackUpClobbersReg(*I, Reg, TRI))
      continue;

    bool HasUse = false;
    bool HasFullDef = false;
    for (const MachineOperand &MO : I->operands()) {
      if (!MO.isReg() || !MO.getReg().isValid())
        continue;
      if (!TRI->regsOverlap(MO.getReg(), Reg))
        continue;
      if (MO.isUse())
        HasUse = true;
      if (MO.isDef()) {
        // Only count as a full kill if the def covers all of Reg.
        // e.g., def $h does NOT kill $hl (L may still be live).
        // def $hl DOES kill $hl and $h and $l.
        if (MO.getReg() == Reg || TRI->isSuperRegister(Reg, MO.getReg()))
          HasFullDef = true;
      }
    }
    if (HasUse)
      return true; // Register is used by this instruction — it's live
    if (HasFullDef)
      return false; // Register is fully defined without use — it's dead
  }
  // No use/def found in remaining instructions — check live-out.
  // At O1+, values may be live across BB boundaries.
  for (const MachineBasicBlock *Succ : MBB.successors()) {
    for (const auto &LI : Succ->liveins()) {
      if (TRI->regsOverlap(LI.PhysReg, Reg))
        return true;
    }
  }
  return false;
}

//===----------------------------------------------------------------------===//
// Large-offset SPILL/RELOAD expansion (IX-based frame pointer)
//===----------------------------------------------------------------------===//

// Expand SPILL_GR8 with large offset.
// Computes address in HL, stores SrcReg via LD (HL),r.
static void expandSpillGR8LargeOffset(MachineBasicBlock &MBB,
                                      MachineBasicBlock::iterator MI,
                                      const DebugLoc &DL,
                                      const TargetInstrInfo &TII,
                                      Register SrcReg, int64_t Offset,
                                      const TargetRegisterInfo *TRI) {
  bool SrcIsHL = isSubRegOf(SrcReg, Z80::HL);
  Register TempReg = chooseTempReg(SrcReg, MBB, MI, TRI);
  bool PreserveFlags = isFlagsLiveAfter(MI, TRI);
  auto NextIt = std::next(MachineBasicBlock::iterator(MI));

  // When spilling H or L, we must always save/restore HL: the other half of
  // the pair may be needed by a subsequent SPILL_GR8 (the register allocator
  // often emits consecutive SPILL_GR8 for L and H from the same killed HL).
  bool NeedSaveHL = SrcIsHL || isRegLiveAt(Z80::HL, MBB, NextIt, TRI);
  bool NeedSaveTemp = isRegLiveAt(TempReg, MBB, NextIt, TRI);
  bool NeedSaveAF = SrcIsHL && isRegLiveAt(Z80::A, MBB, NextIt, TRI);

  // For H/L: copy to A before clobbering HL.
  if (NeedSaveAF)
    BuildMI(MBB, MI, DL, TII.get(Z80::PUSH_AF));
  if (SrcIsHL)
    BuildMI(MBB, MI, DL, TII.get(getCopyToAOpcode(SrcReg)));

  if (NeedSaveHL)
    BuildMI(MBB, MI, DL, TII.get(Z80::PUSH_HL));
  if (NeedSaveTemp)
    BuildMI(MBB, MI, DL, TII.get(getPushOpcode(TempReg)));

  emitLargeOffsetAddr(MBB, MI, DL, TII, Offset, TempReg, PreserveFlags);

  if (SrcIsHL)
    BuildMI(MBB, MI, DL, TII.get(Z80::LD_HLind_A));
  else
    BuildMI(MBB, MI, DL, TII.get(getStoreHLindOpcode(SrcReg)));

  if (NeedSaveTemp)
    BuildMI(MBB, MI, DL, TII.get(getPopOpcode(TempReg)));
  if (NeedSaveHL)
    BuildMI(MBB, MI, DL, TII.get(Z80::POP_HL));
  if (NeedSaveAF)
    BuildMI(MBB, MI, DL, TII.get(Z80::POP_AF));
}

// Expand RELOAD_GR8 with large offset.
// Computes address in HL, loads via LD r,(HL).
static void expandReloadGR8LargeOffset(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator MI,
                                       const DebugLoc &DL,
                                       const TargetInstrInfo &TII,
                                       Register DstReg, int64_t Offset,
                                       const TargetRegisterInfo *TRI) {
  bool DstIsHL = isSubRegOf(DstReg, Z80::HL);
  Register TempReg = chooseTempReg(DstReg, MBB, MI, TRI);
  bool PreserveFlags = isFlagsLiveAfter(MI, TRI);
  auto NextIt = std::next(MachineBasicBlock::iterator(MI));

  // When reloading into H or L, always save/restore HL: consecutive
  // RELOAD_GR8 for H and L need the restored HL so the second reload's
  // "LD DstReg, A" writes into a correctly-preserved register pair.
  bool NeedSaveHL = DstIsHL || isRegLiveAt(Z80::HL, MBB, NextIt, TRI);
  bool NeedSaveTemp = isRegLiveAt(TempReg, MBB, NextIt, TRI);
  bool NeedSaveAF = DstIsHL && isRegLiveAt(Z80::A, MBB, NextIt, TRI);

  if (NeedSaveAF)
    BuildMI(MBB, MI, DL, TII.get(Z80::PUSH_AF));
  if (NeedSaveHL)
    BuildMI(MBB, MI, DL, TII.get(Z80::PUSH_HL));
  if (NeedSaveTemp)
    BuildMI(MBB, MI, DL, TII.get(getPushOpcode(TempReg)));

  emitLargeOffsetAddr(MBB, MI, DL, TII, Offset, TempReg, PreserveFlags);

  if (DstIsHL) {
    // Can't load directly into H/L (HL holds the address).
    // Load into A, restore scratch, then copy A → DstReg.
    BuildMI(MBB, MI, DL, TII.get(Z80::LD_A_HLind));
    if (NeedSaveTemp)
      BuildMI(MBB, MI, DL, TII.get(getPopOpcode(TempReg)));
    if (NeedSaveHL)
      BuildMI(MBB, MI, DL, TII.get(Z80::POP_HL));
    BuildMI(MBB, MI, DL, TII.get(getCopyFromAOpcode(DstReg)));
    if (NeedSaveAF)
      BuildMI(MBB, MI, DL, TII.get(Z80::POP_AF));
  } else {
    BuildMI(MBB, MI, DL, TII.get(getLoadHLindOpcode(DstReg)));
    if (NeedSaveTemp)
      BuildMI(MBB, MI, DL, TII.get(getPopOpcode(TempReg)));
    if (NeedSaveHL)
      BuildMI(MBB, MI, DL, TII.get(Z80::POP_HL));
  }
}

// Expand SPILL_GR16 with large offset.
// Computes address in HL, stores both bytes via LD (HL),lo; INC HL; LD (HL),hi.
static void expandSpillGR16LargeOffset(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator MI,
                                       const DebugLoc &DL,
                                       const TargetInstrInfo &TII,
                                       Register SrcReg, int64_t Offset,
                                       const TargetRegisterInfo *TRI) {
  bool PreserveFlags = isFlagsLiveAfter(MI, TRI);
  auto NextIt = std::next(MachineBasicBlock::iterator(MI));

  if (SrcReg == Z80::HL) {
    // HL is both the value to store and the address register.
    // Strategy: push HL (data), compute address, pop data into temp.
    Register TempReg;
    if (!isRegLiveAt(Z80::DE, MBB, NextIt, TRI))
      TempReg = Z80::DE;
    else
      TempReg = Z80::BC;

    bool NeedSaveTemp = isRegLiveAt(TempReg, MBB, NextIt, TRI);

    if (NeedSaveTemp)
      BuildMI(MBB, MI, DL, TII.get(getPushOpcode(TempReg)));
    BuildMI(MBB, MI, DL, TII.get(Z80::PUSH_HL)); // push data

    emitLargeOffsetAddr(MBB, MI, DL, TII, Offset, TempReg, PreserveFlags);

    // Pop original HL value into TempReg.
    BuildMI(MBB, MI, DL, TII.get(getPopOpcode(TempReg)));

    Register TempLo = (TempReg == Z80::BC) ? Z80::C : Z80::E;
    Register TempHi = (TempReg == Z80::BC) ? Z80::B : Z80::D;
    BuildMI(MBB, MI, DL, TII.get(getStoreHLindOpcode(TempLo)));
    BuildMI(MBB, MI, DL, TII.get(Z80::INC_HL));
    BuildMI(MBB, MI, DL, TII.get(getStoreHLindOpcode(TempHi)));

    // Restore HL from TempReg if the spill didn't kill HL.
    if (!MI->getOperand(0).isKill()) {
      unsigned CopyH = (TempReg == Z80::BC) ? Z80::LD_H_B : Z80::LD_H_D;
      unsigned CopyL = (TempReg == Z80::BC) ? Z80::LD_L_C : Z80::LD_L_E;
      BuildMI(MBB, MI, DL, TII.get(CopyL));
      BuildMI(MBB, MI, DL, TII.get(CopyH));
    }

    if (NeedSaveTemp)
      BuildMI(MBB, MI, DL, TII.get(getPopOpcode(TempReg)));
  } else {
    // SrcReg is BC or DE. Use the other as temp.
    Register TempReg = (SrcReg == Z80::BC) ? Z80::DE : Z80::BC;
    Register SrcLo = (SrcReg == Z80::BC) ? Z80::C : Z80::E;
    Register SrcHi = (SrcReg == Z80::BC) ? Z80::B : Z80::D;

    bool NeedSaveHL = isRegLiveAt(Z80::HL, MBB, NextIt, TRI);
    bool NeedSaveTemp = isRegLiveAt(TempReg, MBB, NextIt, TRI);

    if (NeedSaveHL)
      BuildMI(MBB, MI, DL, TII.get(Z80::PUSH_HL));
    if (NeedSaveTemp)
      BuildMI(MBB, MI, DL, TII.get(getPushOpcode(TempReg)));

    emitLargeOffsetAddr(MBB, MI, DL, TII, Offset, TempReg, PreserveFlags);

    BuildMI(MBB, MI, DL, TII.get(getStoreHLindOpcode(SrcLo)));
    BuildMI(MBB, MI, DL, TII.get(Z80::INC_HL));
    BuildMI(MBB, MI, DL, TII.get(getStoreHLindOpcode(SrcHi)));

    if (NeedSaveTemp)
      BuildMI(MBB, MI, DL, TII.get(getPopOpcode(TempReg)));
    if (NeedSaveHL)
      BuildMI(MBB, MI, DL, TII.get(Z80::POP_HL));
  }
}

// Expand RELOAD_GR16 with large offset.
// Computes address in HL, loads via LD lo,(HL); INC HL; LD hi,(HL).
static void expandReloadGR16LargeOffset(MachineBasicBlock &MBB,
                                        MachineBasicBlock::iterator MI,
                                        const DebugLoc &DL,
                                        const TargetInstrInfo &TII,
                                        Register DstReg, int64_t Offset,
                                        const TargetRegisterInfo *TRI) {
  bool PreserveFlags = isFlagsLiveAfter(MI, TRI);
  auto NextIt = std::next(MachineBasicBlock::iterator(MI));

  if (DstReg == Z80::HL) {
    // HL is the destination, but also used for address computation.
    // Load into a temp pair, then copy to HL.
    Register TempReg;
    if (!isRegLiveAt(Z80::BC, MBB, NextIt, TRI))
      TempReg = Z80::BC;
    else if (!isRegLiveAt(Z80::DE, MBB, NextIt, TRI))
      TempReg = Z80::DE;
    else
      TempReg = Z80::BC;

    bool NeedSaveTemp = isRegLiveAt(TempReg, MBB, NextIt, TRI);

    if (NeedSaveTemp)
      BuildMI(MBB, MI, DL, TII.get(getPushOpcode(TempReg)));

    emitLargeOffsetAddr(MBB, MI, DL, TII, Offset, TempReg, PreserveFlags);

    Register TempLo = (TempReg == Z80::BC) ? Z80::C : Z80::E;
    Register TempHi = (TempReg == Z80::BC) ? Z80::B : Z80::D;
    BuildMI(MBB, MI, DL, TII.get(getLoadHLindOpcode(TempLo)));
    BuildMI(MBB, MI, DL, TII.get(Z80::INC_HL));
    BuildMI(MBB, MI, DL, TII.get(getLoadHLindOpcode(TempHi)));

    // Copy temp to HL.
    unsigned CopyL = (TempReg == Z80::BC) ? Z80::LD_L_C : Z80::LD_L_E;
    unsigned CopyH = (TempReg == Z80::BC) ? Z80::LD_H_B : Z80::LD_H_D;
    BuildMI(MBB, MI, DL, TII.get(CopyL));
    BuildMI(MBB, MI, DL, TII.get(CopyH));

    if (NeedSaveTemp)
      BuildMI(MBB, MI, DL, TII.get(getPopOpcode(TempReg)));
  } else {
    // DstReg is BC or DE. Use DstReg itself as temp (old value is dead).
    bool NeedSaveHL = isRegLiveAt(Z80::HL, MBB, NextIt, TRI);

    if (NeedSaveHL)
      BuildMI(MBB, MI, DL, TII.get(Z80::PUSH_HL));

    emitLargeOffsetAddr(MBB, MI, DL, TII, Offset, DstReg, PreserveFlags);

    Register DstLo = (DstReg == Z80::BC) ? Z80::C : Z80::E;
    Register DstHi = (DstReg == Z80::BC) ? Z80::B : Z80::D;
    BuildMI(MBB, MI, DL, TII.get(getLoadHLindOpcode(DstLo)));
    BuildMI(MBB, MI, DL, TII.get(Z80::INC_HL));
    BuildMI(MBB, MI, DL, TII.get(getLoadHLindOpcode(DstHi)));

    if (NeedSaveHL)
      BuildMI(MBB, MI, DL, TII.get(Z80::POP_HL));
  }
}

//===----------------------------------------------------------------------===//
// SP-relative stack access (used when frame pointer is omitted)
//===----------------------------------------------------------------------===//

// Emit code to compute HL = SP_at_instruction + Offset.
// SPDelta accounts for PUSHes emitted by the CALLER before this point.
// PreserveFlags adds a SEPARATE PUSH AF/POP AF inside this function to
// protect FLAGS from ADD HL,SP. These are independent SP shifts:
//   - SPDelta: caller's PUSHes (e.g., NeedSaveAF, NeedSaveHL)
//   - PreserveFlags +2: this function's own PUSH AF for FLAGS preservation
//
// SM83 optimization: uses LDHL SP,e (0xF8) when the adjusted offset fits
// in a signed 8-bit range (-128..+127). This replaces the 2-instruction
// sequence (LD HL,nn + ADD HL,SP) with a single instruction.
static void emitSPRelativeAddr(MachineBasicBlock &MBB,
                               MachineBasicBlock::iterator InsertBefore,
                               const DebugLoc &DL, const TargetInstrInfo &TII,
                               int64_t Offset, int SPDelta,
                               bool PreserveFlags) {
  int AdjOffset = Offset + SPDelta;
  if (PreserveFlags)
    AdjOffset += 2; // PUSH AF will shift SP by 2

  // SM83: use LDHL SP,e if adjusted offset fits in signed 8-bit.
  // This replaces 2-instruction LD HL,nn + ADD HL,SP with a single LDHL SP,e.
  const auto &STI = MBB.getParent()->getSubtarget<Z80Subtarget>();
  if (STI.hasSM83() && AdjOffset >= -128 && AdjOffset <= 127) {
    if (PreserveFlags)
      BuildMI(MBB, InsertBefore, DL, TII.get(Z80::PUSH_AF));
    BuildMI(MBB, InsertBefore, DL, TII.get(Z80::LDHL_SP_e))
        .addImm(AdjOffset & 0xFF);
    if (PreserveFlags)
      BuildMI(MBB, InsertBefore, DL, TII.get(Z80::POP_AF));
    return;
  }

  // Z80 (and SM83 fallback for large offsets):
  // LD HL,nn; [PUSH AF;] ADD HL,SP; [POP AF;]
  BuildMI(MBB, InsertBefore, DL, TII.get(Z80::LD_HL_nn))
      .addImm(AdjOffset & 0xFFFF);
  if (PreserveFlags)
    BuildMI(MBB, InsertBefore, DL, TII.get(Z80::PUSH_AF));
  BuildMI(MBB, InsertBefore, DL, TII.get(Z80::ADD_HL_SP));
  if (PreserveFlags)
    BuildMI(MBB, InsertBefore, DL, TII.get(Z80::POP_AF));
}

// Expand SPILL_GR8 with SP-relative addressing.
static void expandSpillGR8SPRelative(MachineBasicBlock &MBB,
                                     MachineBasicBlock::iterator MI,
                                     const DebugLoc &DL,
                                     const TargetInstrInfo &TII,
                                     Register SrcReg, int64_t Offset,
                                     const TargetRegisterInfo *TRI) {
  bool SrcIsHL = isSubRegOf(SrcReg, Z80::HL);
  bool PreserveFlags = isFlagsLiveAfter(MI, TRI);
  auto NextIt = std::next(MachineBasicBlock::iterator(MI));
  int SPDelta = 0;

  // If source is H or L, copy to A before clobbering HL for address.
  bool NeedSaveAF = SrcIsHL && isRegLiveAt(Z80::A, MBB, NextIt, TRI);
  if (NeedSaveAF) {
    BuildMI(MBB, MI, DL, TII.get(Z80::PUSH_AF));
    SPDelta += 2;
  }
  if (SrcIsHL)
    BuildMI(MBB, MI, DL, TII.get(getCopyToAOpcode(SrcReg)));

  bool NeedSaveHL = SrcIsHL || isRegLiveAt(Z80::HL, MBB, NextIt, TRI);
  if (NeedSaveHL) {
    BuildMI(MBB, MI, DL, TII.get(Z80::PUSH_HL));
    SPDelta += 2;
  }

  emitSPRelativeAddr(MBB, MI, DL, TII, Offset, SPDelta, PreserveFlags);

  if (SrcIsHL)
    BuildMI(MBB, MI, DL, TII.get(Z80::LD_HLind_A));
  else
    BuildMI(MBB, MI, DL, TII.get(getStoreHLindOpcode(SrcReg)));

  if (NeedSaveHL)
    BuildMI(MBB, MI, DL, TII.get(Z80::POP_HL));
  if (NeedSaveAF)
    BuildMI(MBB, MI, DL, TII.get(Z80::POP_AF));
}

// Expand RELOAD_GR8 with SP-relative addressing.
static void expandReloadGR8SPRelative(MachineBasicBlock &MBB,
                                      MachineBasicBlock::iterator MI,
                                      const DebugLoc &DL,
                                      const TargetInstrInfo &TII,
                                      Register DstReg, int64_t Offset,
                                      const TargetRegisterInfo *TRI) {
  bool DstIsHL = isSubRegOf(DstReg, Z80::HL);
  bool PreserveFlags = isFlagsLiveAfter(MI, TRI);
  auto NextIt = std::next(MachineBasicBlock::iterator(MI));
  int SPDelta = 0;

  bool NeedSaveAF = DstIsHL && isRegLiveAt(Z80::A, MBB, NextIt, TRI);
  if (NeedSaveAF) {
    BuildMI(MBB, MI, DL, TII.get(Z80::PUSH_AF));
    SPDelta += 2;
  }

  bool NeedSaveHL = DstIsHL || isRegLiveAt(Z80::HL, MBB, NextIt, TRI);
  if (NeedSaveHL) {
    BuildMI(MBB, MI, DL, TII.get(Z80::PUSH_HL));
    SPDelta += 2;
  }

  emitSPRelativeAddr(MBB, MI, DL, TII, Offset, SPDelta, PreserveFlags);

  if (DstIsHL) {
    BuildMI(MBB, MI, DL, TII.get(Z80::LD_A_HLind));
    if (NeedSaveHL)
      BuildMI(MBB, MI, DL, TII.get(Z80::POP_HL));
    BuildMI(MBB, MI, DL, TII.get(getCopyFromAOpcode(DstReg)));
    if (NeedSaveAF)
      BuildMI(MBB, MI, DL, TII.get(Z80::POP_AF));
  } else {
    BuildMI(MBB, MI, DL, TII.get(getLoadHLindOpcode(DstReg)));
    if (NeedSaveHL)
      BuildMI(MBB, MI, DL, TII.get(Z80::POP_HL));
  }
}

// Expand SPILL_GR16 with SP-relative addressing.
// Handles BC, DE, HL, and IX (which has no sub-registers).
static void expandSpillGR16SPRelative(MachineBasicBlock &MBB,
                                      MachineBasicBlock::iterator MI,
                                      const DebugLoc &DL,
                                      const TargetInstrInfo &TII,
                                      Register SrcReg, int64_t Offset,
                                      const TargetRegisterInfo *TRI) {
  bool PreserveFlags = isFlagsLiveAfter(MI, TRI);
  auto NextIt = std::next(MachineBasicBlock::iterator(MI));
  int SPDelta = 0;

  if (SrcReg == Z80::IX || SrcReg == Z80::IY) {
    // IX/IY have no accessible sub-registers.
    // Transfer to a temp pair via PUSH/POP, then store bytes.
    Register TempReg;
    if (!isRegLiveAt(Z80::DE, MBB, NextIt, TRI))
      TempReg = Z80::DE;
    else
      TempReg = Z80::BC;
    bool NeedSaveTemp = isRegLiveAt(TempReg, MBB, NextIt, TRI);
    bool NeedSaveHL = isRegLiveAt(Z80::HL, MBB, NextIt, TRI);

    if (NeedSaveHL) {
      BuildMI(MBB, MI, DL, TII.get(Z80::PUSH_HL));
      SPDelta += 2;
    }
    if (NeedSaveTemp) {
      BuildMI(MBB, MI, DL, TII.get(getPushOpcode(TempReg)));
      SPDelta += 2;
    }

    // Transfer IX/IY to TempReg via PUSH/POP
    unsigned PushOpc = (SrcReg == Z80::IX) ? Z80::PUSH_IX : Z80::PUSH_IY;
    BuildMI(MBB, MI, DL, TII.get(PushOpc));
    SPDelta += 2;
    BuildMI(MBB, MI, DL, TII.get(getPopOpcode(TempReg)));
    SPDelta -= 2;

    Register TempLo = (TempReg == Z80::BC) ? Z80::C : Z80::E;
    Register TempHi = (TempReg == Z80::BC) ? Z80::B : Z80::D;

    emitSPRelativeAddr(MBB, MI, DL, TII, Offset, SPDelta, PreserveFlags);

    BuildMI(MBB, MI, DL, TII.get(getStoreHLindOpcode(TempLo)));
    BuildMI(MBB, MI, DL, TII.get(Z80::INC_HL));
    BuildMI(MBB, MI, DL, TII.get(getStoreHLindOpcode(TempHi)));

    if (NeedSaveTemp)
      BuildMI(MBB, MI, DL, TII.get(getPopOpcode(TempReg)));
    if (NeedSaveHL)
      BuildMI(MBB, MI, DL, TII.get(Z80::POP_HL));
  } else if (SrcReg == Z80::HL) {
    // HL is both source and address register.
    Register TempReg;
    if (!isRegLiveAt(Z80::DE, MBB, NextIt, TRI))
      TempReg = Z80::DE;
    else
      TempReg = Z80::BC;
    bool NeedSaveTemp = isRegLiveAt(TempReg, MBB, NextIt, TRI);

    if (NeedSaveTemp) {
      BuildMI(MBB, MI, DL, TII.get(getPushOpcode(TempReg)));
      SPDelta += 2;
    }
    // Copy HL data to TempReg via LD r,r'
    Register TempLo = (TempReg == Z80::BC) ? Z80::C : Z80::E;
    Register TempHi = (TempReg == Z80::BC) ? Z80::B : Z80::D;
    unsigned CopyLo = (TempReg == Z80::BC) ? Z80::LD_C_L : Z80::LD_E_L;
    unsigned CopyHi = (TempReg == Z80::BC) ? Z80::LD_B_H : Z80::LD_D_H;
    BuildMI(MBB, MI, DL, TII.get(CopyLo));
    BuildMI(MBB, MI, DL, TII.get(CopyHi));

    emitSPRelativeAddr(MBB, MI, DL, TII, Offset, SPDelta, PreserveFlags);

    BuildMI(MBB, MI, DL, TII.get(getStoreHLindOpcode(TempLo)));
    BuildMI(MBB, MI, DL, TII.get(Z80::INC_HL));
    BuildMI(MBB, MI, DL, TII.get(getStoreHLindOpcode(TempHi)));

    if (!MI->getOperand(0).isKill()) {
      // Restore HL from TempReg
      unsigned RestL = (TempReg == Z80::BC) ? Z80::LD_L_C : Z80::LD_L_E;
      unsigned RestH = (TempReg == Z80::BC) ? Z80::LD_H_B : Z80::LD_H_D;
      BuildMI(MBB, MI, DL, TII.get(RestL));
      BuildMI(MBB, MI, DL, TII.get(RestH));
    }

    if (NeedSaveTemp)
      BuildMI(MBB, MI, DL, TII.get(getPopOpcode(TempReg)));
  } else {
    // SrcReg is BC or DE.
    Register SrcLo = TRI->getSubReg(SrcReg, Z80::sub_lo);
    Register SrcHi = TRI->getSubReg(SrcReg, Z80::sub_hi);
    bool NeedSaveHL = isRegLiveAt(Z80::HL, MBB, NextIt, TRI);

    if (NeedSaveHL) {
      BuildMI(MBB, MI, DL, TII.get(Z80::PUSH_HL));
      SPDelta += 2;
    }

    emitSPRelativeAddr(MBB, MI, DL, TII, Offset, SPDelta, PreserveFlags);

    BuildMI(MBB, MI, DL, TII.get(getStoreHLindOpcode(SrcLo)));
    BuildMI(MBB, MI, DL, TII.get(Z80::INC_HL));
    BuildMI(MBB, MI, DL, TII.get(getStoreHLindOpcode(SrcHi)));

    if (NeedSaveHL)
      BuildMI(MBB, MI, DL, TII.get(Z80::POP_HL));
  }
}

// Expand RELOAD_GR16 with SP-relative addressing.
static void expandReloadGR16SPRelative(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator MI,
                                       const DebugLoc &DL,
                                       const TargetInstrInfo &TII,
                                       Register DstReg, int64_t Offset,
                                       const TargetRegisterInfo *TRI) {
  bool PreserveFlags = isFlagsLiveAfter(MI, TRI);
  auto NextIt = std::next(MachineBasicBlock::iterator(MI));
  int SPDelta = 0;

  if (DstReg == Z80::IX || DstReg == Z80::IY) {
    // Load into temp pair, then transfer to IX/IY.
    Register TempReg;
    if (!isRegLiveAt(Z80::BC, MBB, NextIt, TRI))
      TempReg = Z80::BC;
    else if (!isRegLiveAt(Z80::DE, MBB, NextIt, TRI))
      TempReg = Z80::DE;
    else
      TempReg = Z80::BC;
    bool NeedSaveTemp = isRegLiveAt(TempReg, MBB, NextIt, TRI);
    bool NeedSaveHL = isRegLiveAt(Z80::HL, MBB, NextIt, TRI);

    if (NeedSaveHL) {
      BuildMI(MBB, MI, DL, TII.get(Z80::PUSH_HL));
      SPDelta += 2;
    }
    if (NeedSaveTemp) {
      BuildMI(MBB, MI, DL, TII.get(getPushOpcode(TempReg)));
      SPDelta += 2;
    }

    emitSPRelativeAddr(MBB, MI, DL, TII, Offset, SPDelta, PreserveFlags);

    Register TempLo = (TempReg == Z80::BC) ? Z80::C : Z80::E;
    Register TempHi = (TempReg == Z80::BC) ? Z80::B : Z80::D;
    BuildMI(MBB, MI, DL, TII.get(getLoadHLindOpcode(TempLo)));
    BuildMI(MBB, MI, DL, TII.get(Z80::INC_HL));
    BuildMI(MBB, MI, DL, TII.get(getLoadHLindOpcode(TempHi)));

    // Transfer TempReg to IX/IY via PUSH/POP
    BuildMI(MBB, MI, DL, TII.get(getPushOpcode(TempReg)));
    unsigned PopOpc = (DstReg == Z80::IX) ? Z80::POP_IX : Z80::POP_IY;
    BuildMI(MBB, MI, DL, TII.get(PopOpc));

    if (NeedSaveTemp)
      BuildMI(MBB, MI, DL, TII.get(getPopOpcode(TempReg)));
    if (NeedSaveHL)
      BuildMI(MBB, MI, DL, TII.get(Z80::POP_HL));
  } else if (DstReg == Z80::HL) {
    // Load into temp pair, then copy to HL.
    Register TempReg;
    if (!isRegLiveAt(Z80::BC, MBB, NextIt, TRI))
      TempReg = Z80::BC;
    else if (!isRegLiveAt(Z80::DE, MBB, NextIt, TRI))
      TempReg = Z80::DE;
    else
      TempReg = Z80::BC;
    bool NeedSaveTemp = isRegLiveAt(TempReg, MBB, NextIt, TRI);

    if (NeedSaveTemp) {
      BuildMI(MBB, MI, DL, TII.get(getPushOpcode(TempReg)));
      SPDelta += 2;
    }

    emitSPRelativeAddr(MBB, MI, DL, TII, Offset, SPDelta, PreserveFlags);

    Register TempLo = (TempReg == Z80::BC) ? Z80::C : Z80::E;
    Register TempHi = (TempReg == Z80::BC) ? Z80::B : Z80::D;
    BuildMI(MBB, MI, DL, TII.get(getLoadHLindOpcode(TempLo)));
    BuildMI(MBB, MI, DL, TII.get(Z80::INC_HL));
    BuildMI(MBB, MI, DL, TII.get(getLoadHLindOpcode(TempHi)));

    // Copy temp to HL
    unsigned CopyL = (TempReg == Z80::BC) ? Z80::LD_L_C : Z80::LD_L_E;
    unsigned CopyH = (TempReg == Z80::BC) ? Z80::LD_H_B : Z80::LD_H_D;
    BuildMI(MBB, MI, DL, TII.get(CopyL));
    BuildMI(MBB, MI, DL, TII.get(CopyH));

    if (NeedSaveTemp)
      BuildMI(MBB, MI, DL, TII.get(getPopOpcode(TempReg)));
  } else {
    // DstReg is BC or DE. Use it as its own temp for the load.
    bool NeedSaveHL = isRegLiveAt(Z80::HL, MBB, NextIt, TRI);

    if (NeedSaveHL) {
      BuildMI(MBB, MI, DL, TII.get(Z80::PUSH_HL));
      SPDelta += 2;
    }

    emitSPRelativeAddr(MBB, MI, DL, TII, Offset, SPDelta, PreserveFlags);

    Register DstLo = TRI->getSubReg(DstReg, Z80::sub_lo);
    Register DstHi = TRI->getSubReg(DstReg, Z80::sub_hi);
    BuildMI(MBB, MI, DL, TII.get(getLoadHLindOpcode(DstLo)));
    BuildMI(MBB, MI, DL, TII.get(Z80::INC_HL));
    BuildMI(MBB, MI, DL, TII.get(getLoadHLindOpcode(DstHi)));

    if (NeedSaveHL)
      BuildMI(MBB, MI, DL, TII.get(Z80::POP_HL));
  }
}

//===----------------------------------------------------------------------===//
// eliminateFrameIndex
//===----------------------------------------------------------------------===//

bool Z80RegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator MI,
                                          int SPAdj, unsigned FIOperandNum,
                                          RegScavenger *RS) const {
  // RS is unused — we use isRegLiveAt() for accurate liveness checks
  // instead of RegScavenger, which is unreliable with forward frame index
  // elimination (eliminateFrameIndicesBackwards=false).
  (void)RS;

  MachineFunction &MF = *MI->getMF();
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const TargetFrameLowering *TFI = getFrameLowering(MF);

  int Idx = MI->getOperand(FIOperandNum).getIndex();
  int64_t Offset = MFI.getObjectOffset(Idx);

  bool UseFP = TFI->hasFP(MF);

  // Z80 stack frame layout (with frame pointer):
  //   [parameters]     (IX+4, IX+5, ...)  - passed by caller
  //   [return address] (IX+2, IX+3)       - pushed by CALL
  //   [saved IX]       (IX+0, IX+1)       - pushed in prologue, IX points here
  //   [local var 1]    (IX-2, IX-1)       - allocated in prologue
  //   [local var 2]    (IX-4, IX-3)       <- SP after allocation
  //
  // Without frame pointer (SP-relative):
  //   [parameters]     (SP + StackSize + 2, ...)
  //   [return address] (SP + StackSize)
  //   [callee saves]   (SP + LocalSize, ...)
  //   [local var 1]    (SP + LocalSize - 2)
  //   [local var 2]    <- SP

  if (UseFP) {
    Offset += 2; // Skip saved IX
  } else {
    // For callee-cleanup calls, if regalloc inserted this frame-index
    // instruction between CALL and ADJCALLSTACKUP, PEI's SPAdj still
    // includes the arg PUSHes even though the callee already popped them.
    // Detect this by scanning forward: if we hit ADJCALLSTACKUP before
    // any CALL, we're in the post-CALL region and must subtract the
    // callee-cleanup amount.
    int AdjustedSPAdj = SPAdj;
    if (SPAdj > 0) {
      auto It = std::next(MI->getIterator());
      auto End = MI->getParent()->end();
      for (; It != End; ++It) {
        if (It->getOpcode() == Z80::ADJCALLSTACKUP) {
          int CalleeAmount = It->getOperand(1).getImm();
          if (CalleeAmount > 0) {
            LLVM_DEBUG(dbgs() << "  CalleeSPAdj: SPAdj " << SPAdj << " -> "
                              << (SPAdj - CalleeAmount) << " (callee cleanup "
                              << CalleeAmount << ") for FI#" << Idx << "\n");
          }
          AdjustedSPAdj -= CalleeAmount;
          break;
        }
        if (It->isCall()) {
          LLVM_DEBUG(dbgs() << "  PreCALL region: SPAdj " << SPAdj
                            << " kept for FI#" << Idx << "\n");
          break; // Pre-CALL region, SPAdj is correct
        }
      }
    }
    Offset += MFI.getStackSize() + AdjustedSPAdj;
    // PEI's StackSize excludes LocalAreaOffset (return address size), but
    // regular objects' offsets include that bias. Compensate for non-fixed
    // objects. Fixed objects (negative Idx, e.g. stack args) use raw
    // SP-entry-relative offsets and don't need this adjustment.
    if (Idx >= 0)
      Offset -= TFI->getOffsetOfLocalArea(); // -(-2) = +2 for Z80
    LLVM_DEBUG(dbgs() << "  FI#" << Idx << " SPAdj=" << SPAdj << " AdjSPAdj="
                      << AdjustedSPAdj << " StackSize=" << MFI.getStackSize()
                      << " ObjOff=" << MFI.getObjectOffset(Idx)
                      << " FinalOffset=" << Offset << " in " << MF.getName()
                      << " bb." << MI->getParent()->getNumber() << "\n");
  }

  // Add any additional offset from the instruction operand
  // (for accessing bytes within a multi-byte stack slot)
  if (FIOperandNum + 1 < MI->getNumOperands() &&
      MI->getOperand(FIOperandNum + 1).isImm()) {
    Offset += MI->getOperand(FIOperandNum + 1).getImm();
    MI->removeOperand(FIOperandNum + 1);
  }

  unsigned Opc = MI->getOpcode();
  MachineBasicBlock &MBB = *MI->getParent();
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  DebugLoc DL = MI->getDebugLoc();

  // LEA_IX_FI: compute the actual address of a stack object into a register.
  if (Opc == Z80::LEA_IX_FI) {
    Register DstReg = MI->getOperand(0).getReg();

    if (!UseFP) {
      // SP-relative: LD HL, Offset; ADD HL, SP; then copy to DstReg.
      bool PreserveFlags = isFlagsLiveAfter(MI, this);
      int SPDelta = 0;

      if (DstReg == Z80::HL) {
        emitSPRelativeAddr(MBB, MI, DL, TII, Offset, 0, PreserveFlags);
      } else {
        bool NeedSaveHL = isRegLiveAt(Z80::HL, MBB, std::next(MI), this);
        if (NeedSaveHL) {
          BuildMI(MBB, MI, DL, TII.get(Z80::PUSH_HL));
          SPDelta += 2;
        }
        emitSPRelativeAddr(MBB, MI, DL, TII, Offset, SPDelta, PreserveFlags);
        if (DstReg == Z80::DE) {
          const auto &STI = MF.getSubtarget<Z80Subtarget>();
          if (STI.hasSM83()) {
            BuildMI(MBB, MI, DL, TII.get(Z80::LD_D_H));
            BuildMI(MBB, MI, DL, TII.get(Z80::LD_E_L));
          } else {
            BuildMI(MBB, MI, DL, TII.get(Z80::EX_DE_HL));
          }
        } else if (DstReg == Z80::BC) {
          BuildMI(MBB, MI, DL, TII.get(Z80::LD_B_H));
          BuildMI(MBB, MI, DL, TII.get(Z80::LD_C_L));
        } else if (DstReg == Z80::IX) {
          BuildMI(MBB, MI, DL, TII.get(Z80::PUSH_HL));
          BuildMI(MBB, MI, DL, TII.get(Z80::POP_IX));
        } else {
          llvm_unreachable("Unexpected register for LEA_IX_FI");
        }
        if (NeedSaveHL)
          BuildMI(MBB, MI, DL, TII.get(Z80::POP_HL));
      }
      MI->eraseFromParent();
      return false;
    }

    // IX-based (hasFP): existing logic
    if (Offset == 0) {
      BuildMI(MBB, MI, DL, TII.get(Z80::PUSH_IX));
      if (DstReg == Z80::HL)
        BuildMI(MBB, MI, DL, TII.get(Z80::POP_HL));
      else if (DstReg == Z80::DE)
        BuildMI(MBB, MI, DL, TII.get(Z80::POP_DE));
      else if (DstReg == Z80::BC)
        BuildMI(MBB, MI, DL, TII.get(Z80::POP_BC));
      else
        llvm_unreachable("Unexpected register for LEA_IX_FI");
    } else if (DstReg == Z80::HL) {
      bool PreserveFlags = isFlagsLiveAfter(MI, this);
      auto NextIt = std::next(MI);
      Register TempReg;
      if (!isRegLiveAt(Z80::BC, MBB, NextIt, this))
        TempReg = Z80::BC;
      else if (!isRegLiveAt(Z80::DE, MBB, NextIt, this))
        TempReg = Z80::DE;
      else
        TempReg = Z80::BC;
      bool NeedSaveTemp = isRegLiveAt(TempReg, MBB, NextIt, this);

      if (NeedSaveTemp)
        BuildMI(MBB, MI, DL, TII.get(getPushOpcode(TempReg)));
      emitLargeOffsetAddr(MBB, MI, DL, TII, Offset, TempReg, PreserveFlags);
      if (NeedSaveTemp)
        BuildMI(MBB, MI, DL, TII.get(getPopOpcode(TempReg)));
    } else if (DstReg == Z80::DE) {
      bool PreserveFlags = isFlagsLiveAfter(MI, this);
      bool NeedSaveHL = isRegLiveAt(Z80::HL, MBB, std::next(MI), this);

      if (NeedSaveHL)
        BuildMI(MBB, MI, DL, TII.get(Z80::PUSH_HL));
      emitLargeOffsetAddr(MBB, MI, DL, TII, Offset, Z80::DE, PreserveFlags);
      BuildMI(MBB, MI, DL, TII.get(Z80::EX_DE_HL));
      if (NeedSaveHL)
        BuildMI(MBB, MI, DL, TII.get(Z80::POP_HL));
    } else if (DstReg == Z80::BC) {
      bool PreserveFlags = isFlagsLiveAfter(MI, this);
      bool NeedSaveHL = isRegLiveAt(Z80::HL, MBB, std::next(MI), this);

      if (NeedSaveHL)
        BuildMI(MBB, MI, DL, TII.get(Z80::PUSH_HL));
      emitLargeOffsetAddr(MBB, MI, DL, TII, Offset, Z80::BC, PreserveFlags);
      BuildMI(MBB, MI, DL, TII.get(Z80::LD_B_H));
      BuildMI(MBB, MI, DL, TII.get(Z80::LD_C_L));
      if (NeedSaveHL)
        BuildMI(MBB, MI, DL, TII.get(Z80::POP_HL));
    } else {
      llvm_unreachable("Unexpected register for LEA_IX_FI");
    }
    MI->eraseFromParent();
    return false;
  }

  // --- SP-relative mode (no frame pointer) ---
  // All SPILL/RELOAD must be expanded inline; IX+d is not available.
  if (!UseFP) {
    if (Opc == Z80::SPILL_IMM8) {
      int64_t Val = MI->getOperand(0).getImm();
      bool PreserveFlags = isFlagsLiveAfter(MI, this);
      bool NeedSaveHL = isRegLiveAt(Z80::HL, MBB, std::next(MI), this);
      int SPDelta = 0;

      if (NeedSaveHL) {
        BuildMI(MBB, MI, DL, TII.get(Z80::PUSH_HL));
        SPDelta += 2;
      }
      emitSPRelativeAddr(MBB, MI, DL, TII, Offset, SPDelta, PreserveFlags);
      BuildMI(MBB, MI, DL, TII.get(Z80::LD_HLind_n)).addImm(Val & 0xFF);
      if (NeedSaveHL)
        BuildMI(MBB, MI, DL, TII.get(Z80::POP_HL));
      MI->eraseFromParent();
      return false;
    }

    if (Opc == Z80::SPILL_GR8) {
      Register SrcReg = MI->getOperand(0).getReg();
      expandSpillGR8SPRelative(MBB, MI, DL, TII, SrcReg, Offset, this);
      MI->eraseFromParent();
      return false;
    }

    if (Opc == Z80::RELOAD_GR8) {
      Register DstReg = MI->getOperand(0).getReg();
      expandReloadGR8SPRelative(MBB, MI, DL, TII, DstReg, Offset, this);
      MI->eraseFromParent();
      return false;
    }

    if (Opc == Z80::SPILL_GR16) {
      Register SrcReg = MI->getOperand(0).getReg();
      expandSpillGR16SPRelative(MBB, MI, DL, TII, SrcReg, Offset, this);
      MI->eraseFromParent();
      return false;
    }

    if (Opc == Z80::RELOAD_GR16) {
      Register DstReg = MI->getOperand(0).getReg();
      expandReloadGR16SPRelative(MBB, MI, DL, TII, DstReg, Offset, this);
      MI->eraseFromParent();
      return false;
    }

    // SP-relative mode: unfold ADD_HL_FI/SUB_HL_FI back to RELOAD + op.
    //
    // Without a frame pointer, IX+d addressing is unavailable, so we cannot
    // expand to the 8-bit IX-indexed ALU sequence.  Instead we unfold back
    // to the equivalent of RELOAD_GR16 + ADD_HL_rr / SUB_HL_rr:
    //
    //   [PUSH TempReg]          ; save if TempReg is live
    //   PUSH HL                 ; save running sum (HL is clobbered by reload)
    //   <reload SP-relative>    ; load stack variable into TempReg
    //   POP HL                  ; restore running sum
    //   ADD HL, TempReg         ; (or AND A / SBC HL, TempReg for SUB)
    //   [POP TempReg]           ; restore if was saved
    //
    // The PUSH/POP of HL shifts SP, so Offset is adjusted by SPAdj to
    // compensate for the extra stack entries between SP and the target slot.
    if (Opc == Z80::ADD_HL_FI || Opc == Z80::SUB_HL_FI) {
      auto NextIt = std::next(MI);
      Register TempReg = !isRegLiveAt(Z80::BC, MBB, NextIt, this)   ? Z80::BC
                         : !isRegLiveAt(Z80::DE, MBB, NextIt, this) ? Z80::DE
                                                                    : Z80::BC;
      bool NeedSaveTemp = isRegLiveAt(TempReg, MBB, NextIt, this);

      if (NeedSaveTemp)
        BuildMI(MBB, MI, DL, TII.get(getPushOpcode(TempReg)));
      BuildMI(MBB, MI, DL, TII.get(Z80::PUSH_HL));

      int SPAdj = 2 + (NeedSaveTemp ? 2 : 0);
      expandReloadGR16SPRelative(MBB, MI, DL, TII, TempReg, Offset + SPAdj,
                                 this);

      BuildMI(MBB, MI, DL, TII.get(Z80::POP_HL));

      if (Opc == Z80::ADD_HL_FI) {
        BuildMI(MBB, MI, DL,
                TII.get(TempReg == Z80::BC ? Z80::ADD_HL_BC : Z80::ADD_HL_DE));
      } else {
        BuildMI(MBB, MI, DL, TII.get(Z80::AND_A));
        BuildMI(MBB, MI, DL,
                TII.get(TempReg == Z80::BC ? Z80::SBC_HL_BC : Z80::SBC_HL_DE));
      }

      if (NeedSaveTemp)
        BuildMI(MBB, MI, DL, TII.get(getPopOpcode(TempReg)));

      MI->eraseFromParent();
      return false;
    }

    llvm_unreachable("Unexpected frame index instruction in SP-relative mode");
  }

  // --- Frame pointer mode (IX+d) ---

  // Small offset: fits in IX+d signed 8-bit displacement (-128 to +127).
  // For 16-bit SPILL/RELOAD, both Offset and Offset+1 must fit.
  bool Is16BitFI = (Opc == Z80::SPILL_GR16 || Opc == Z80::RELOAD_GR16 ||
                    Opc == Z80::ADD_HL_FI || Opc == Z80::SUB_HL_FI);
  int64_t MaxOffset = Is16BitFI ? Offset + 1 : Offset;

  if (Offset >= -128 && MaxOffset <= 127) {
    // Expand ADD_HL_FI/SUB_HL_FI to 8-bit IX-indexed ALU sequence (10 bytes).
    // This is the fast path — the offset fits in IX+d, so we can directly
    // use ADD A,(IX+d) / ADC A,(IX+d+1) to perform 16-bit addition byte
    // by byte without allocating a GR16_BCDE register pair.
    //
    //   LD A, L              ; 1B  low byte of running sum
    //   ADD A, (IX+d)        ; 3B  add low byte of stack variable
    //   LD L, A              ; 1B  store back
    //   LD A, H              ; 1B  high byte of running sum
    //   ADC A, (IX+d+1)      ; 3B  add high byte with carry
    //   LD H, A              ; 1B  store back → HL = sum + variable
    if (Opc == Z80::ADD_HL_FI) {
      BuildMI(MBB, MI, DL, TII.get(Z80::LD_A_L));
      BuildMI(MBB, MI, DL, TII.get(Z80::ADD_A_IXd)).addImm(Offset);
      BuildMI(MBB, MI, DL, TII.get(Z80::LD_L_A));
      BuildMI(MBB, MI, DL, TII.get(Z80::LD_A_H));
      BuildMI(MBB, MI, DL, TII.get(Z80::ADC_A_IXd)).addImm(Offset + 1);
      BuildMI(MBB, MI, DL, TII.get(Z80::LD_H_A));
      MI->eraseFromParent();
      return false;
    }
    if (Opc == Z80::SUB_HL_FI) {
      BuildMI(MBB, MI, DL, TII.get(Z80::LD_A_L));
      BuildMI(MBB, MI, DL, TII.get(Z80::SUB_IXd)).addImm(Offset);
      BuildMI(MBB, MI, DL, TII.get(Z80::LD_L_A));
      BuildMI(MBB, MI, DL, TII.get(Z80::LD_A_H));
      BuildMI(MBB, MI, DL, TII.get(Z80::SBC_A_IXd)).addImm(Offset + 1);
      BuildMI(MBB, MI, DL, TII.get(Z80::LD_H_A));
      MI->eraseFromParent();
      return false;
    }

    MI->getOperand(FIOperandNum).ChangeToImmediate(Offset);
    return false;
  }

  // Large offset: expand SPILL/RELOAD with IX-based address computation.
  if (Opc == Z80::SPILL_IMM8) {
    int64_t Val = MI->getOperand(0).getImm();
    auto NextIt = std::next(MI);
    Register TempReg =
        !isRegLiveAt(Z80::BC, MBB, NextIt, this) ? Z80::BC : Z80::DE;
    bool PreserveFlags = isFlagsLiveAfter(MI, this);
    bool NeedSaveHL = isRegLiveAt(Z80::HL, MBB, NextIt, this);
    bool NeedSaveTemp = isRegLiveAt(TempReg, MBB, NextIt, this);

    if (NeedSaveHL)
      BuildMI(MBB, MI, DL, TII.get(Z80::PUSH_HL));
    if (NeedSaveTemp)
      BuildMI(MBB, MI, DL, TII.get(getPushOpcode(TempReg)));
    emitLargeOffsetAddr(MBB, MI, DL, TII, Offset, TempReg, PreserveFlags);
    BuildMI(MBB, MI, DL, TII.get(Z80::LD_HLind_n)).addImm(Val & 0xFF);
    if (NeedSaveTemp)
      BuildMI(MBB, MI, DL, TII.get(getPopOpcode(TempReg)));
    if (NeedSaveHL)
      BuildMI(MBB, MI, DL, TII.get(Z80::POP_HL));
    MI->eraseFromParent();
    return false;
  }

  if (Opc == Z80::SPILL_GR8) {
    Register SrcReg = MI->getOperand(0).getReg();
    expandSpillGR8LargeOffset(MBB, MI, DL, TII, SrcReg, Offset, this);
    MI->eraseFromParent();
    return false;
  }

  if (Opc == Z80::RELOAD_GR8) {
    Register DstReg = MI->getOperand(0).getReg();
    expandReloadGR8LargeOffset(MBB, MI, DL, TII, DstReg, Offset, this);
    MI->eraseFromParent();
    return false;
  }

  if (Opc == Z80::SPILL_GR16) {
    Register SrcReg = MI->getOperand(0).getReg();
    expandSpillGR16LargeOffset(MBB, MI, DL, TII, SrcReg, Offset, this);
    MI->eraseFromParent();
    return false;
  }

  if (Opc == Z80::RELOAD_GR16) {
    Register DstReg = MI->getOperand(0).getReg();
    expandReloadGR16LargeOffset(MBB, MI, DL, TII, DstReg, Offset, this);
    MI->eraseFromParent();
    return false;
  }

  // Large offset fallback for ADD_HL_FI/SUB_HL_FI.
  //
  // When Offset+1 > 127 or Offset < -128, the displacement doesn't fit in
  // IX+d (signed 8-bit), so we cannot use the 10-byte IX-indexed ALU
  // expansion.  Instead we unfold back to the equivalent of a full 16-bit
  // RELOAD + ADD HL,rr, which requires a register pair (TempReg = BC or DE).
  //
  // This is strictly worse than not folding at all (~16B vs ~14B for the
  // unfold), because fold's purpose — avoiding a GR16_BCDE allocation — is
  // defeated by the unfold.  However, the final offset is unknown at fold
  // time (ISel/pre-StackColoring), so we cannot prevent folding for large
  // frames.  This path exists purely as a correctness fallback.
  //
  // In practice this is dead code: Z80 stack frames rarely exceed 127 bytes.
  //
  // Sequence (ADD_HL_FI, TempReg = BC):
  //   [PUSH BC]             ; save if BC is live
  //   PUSH HL               ; save running sum
  //   PUSH IX / POP HL      ;   HL = IX (frame pointer)
  //   LD BC, #Offset        ;   BC = large offset
  //   ADD HL, BC            ;   HL = IX + Offset = &variable
  //   LD C, (HL)            ;   load low byte
  //   INC HL
  //   LD B, (HL)            ;   load high byte → BC = variable value
  //   POP HL                ; restore running sum
  //   ADD HL, BC            ; HL += variable
  //   [POP BC]              ; restore if was saved
  if (Opc == Z80::ADD_HL_FI || Opc == Z80::SUB_HL_FI) {
    bool PreserveFlags = isFlagsLiveAfter(MI, this);
    auto NextIt = std::next(MI);
    Register TempReg = !isRegLiveAt(Z80::BC, MBB, NextIt, this)   ? Z80::BC
                       : !isRegLiveAt(Z80::DE, MBB, NextIt, this) ? Z80::DE
                                                                  : Z80::BC;
    bool NeedSaveTemp = isRegLiveAt(TempReg, MBB, NextIt, this);

    if (NeedSaveTemp)
      BuildMI(MBB, MI, DL, TII.get(getPushOpcode(TempReg)));

    // Save HL (the value to add/sub to).
    BuildMI(MBB, MI, DL, TII.get(Z80::PUSH_HL));

    // Compute address: HL = IX + Offset (clobbers HL)
    emitLargeOffsetAddr(MBB, MI, DL, TII, Offset, TempReg, PreserveFlags);

    // Load from (HL) into TempReg
    Register TempLo = (TempReg == Z80::BC) ? Z80::C : Z80::E;
    Register TempHi = (TempReg == Z80::BC) ? Z80::B : Z80::D;
    BuildMI(MBB, MI, DL, TII.get(getLoadHLindOpcode(TempLo)));
    BuildMI(MBB, MI, DL, TII.get(Z80::INC_HL));
    BuildMI(MBB, MI, DL, TII.get(getLoadHLindOpcode(TempHi)));

    // Restore HL.
    BuildMI(MBB, MI, DL, TII.get(Z80::POP_HL));

    // Perform the 16-bit operation.
    if (Opc == Z80::ADD_HL_FI) {
      BuildMI(MBB, MI, DL,
              TII.get(TempReg == Z80::BC ? Z80::ADD_HL_BC : Z80::ADD_HL_DE));
    } else {
      BuildMI(MBB, MI, DL, TII.get(Z80::AND_A));
      BuildMI(MBB, MI, DL,
              TII.get(TempReg == Z80::BC ? Z80::SBC_HL_BC : Z80::SBC_HL_DE));
    }

    if (NeedSaveTemp)
      BuildMI(MBB, MI, DL, TII.get(getPopOpcode(TempReg)));

    MI->eraseFromParent();
    return false;
  }

  llvm_unreachable("Large frame offset on non-SPILL/RELOAD instruction");
}

Register Z80RegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  const TargetFrameLowering *TFI = getFrameLowering(MF);
  return TFI->hasFP(MF) ? Z80::IX : Z80::SP;
}

StringRef Z80RegisterInfo::getRegAsmName(MCRegister Reg) const {
  switch (Reg.id()) {
  case Z80::A:
    return "a";
  case Z80::B:
    return "b";
  case Z80::C:
    return "c";
  case Z80::D:
    return "d";
  case Z80::E:
    return "e";
  case Z80::H:
    return "h";
  case Z80::L:
    return "l";
  case Z80::BC:
    return "bc";
  case Z80::DE:
    return "de";
  case Z80::HL:
    return "hl";
  case Z80::IX:
    return "ix";
  case Z80::IY:
    return "iy";
  case Z80::SP:
    return "sp";
  default:
    return "";
  }
}
