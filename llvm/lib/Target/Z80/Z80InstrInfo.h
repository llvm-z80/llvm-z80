//===-- Z80InstrInfo.h - Z80 Instruction Information ------------*- C++ -*-===//
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

#ifndef LLVM_LIB_TARGET_Z80_Z80INSTRINFO_H
#define LLVM_LIB_TARGET_Z80_Z80INSTRINFO_H

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

#define GET_INSTRINFO_HEADER
#include "Z80GenInstrInfo.inc"

namespace llvm {

class Z80Subtarget;

namespace Z80 {
/// Mark a register the instruction reads only incidentally as undef: the
/// result does not depend on the value (SBC A,A spreads carry whatever A
/// holds, AND A only clears carry, a flag-save PUSH AF only carries F), so
/// liveness must not demand a prior definition.
inline void markUndefUse(const MachineInstrBuilder &MIB, MCRegister Reg) {
  for (MachineOperand &MO : MIB.getInstr()->operands())
    if (MO.isReg() && MO.isUse() && MO.getReg() == Reg)
      MO.setIsUndef();
}

/// Emit a PUSH HL that saves HL across an inserted sequence. Register
/// allocation may have marked an earlier use of HL as a kill (or its def as
/// dead); this late insertion reads HL after that point, so the stale flag
/// on the nearest HL access must be cleared. If the block never touches HL
/// and it is not live-in, the read carries no value and is marked undef.
inline void emitHLSavePush(MachineBasicBlock &MBB,
                           MachineBasicBlock::iterator InsertBefore,
                           const DebugLoc &DL, const TargetInstrInfo &TII) {
  const TargetRegisterInfo *TRI =
      MBB.getParent()->getSubtarget().getRegisterInfo();
  MachineInstrBuilder Push =
      BuildMI(MBB, InsertBefore, DL, TII.get(Z80::PUSH_HL));
  for (MachineBasicBlock::iterator I = Push->getIterator();
       I != MBB.begin();) {
    --I;
    bool Touched = false;
    for (MachineOperand &MO : I->operands()) {
      if (!MO.isReg() || !MO.getReg().isValid() ||
          !TRI->regsOverlap(MO.getReg(), Z80::HL))
        continue;
      Touched = true;
      if (MO.isDef())
        MO.setIsDead(false);
      else if (MO.isKill())
        MO.setIsKill(false);
    }
    if (Touched)
      return;
  }
  if (!MBB.isLiveIn(Z80::HL) && !MBB.isLiveIn(Z80::L) && !MBB.isLiveIn(Z80::H))
    Z80::markUndefUse(Push, Z80::HL);
}
} // namespace Z80

class Z80InstrInfo : public Z80GenInstrInfo {
public:
  Z80InstrInfo(const Z80Subtarget &STI);

  Register isLoadFromStackSlot(const MachineInstr &MI,
                               int &FrameIndex) const override;

  Register isStoreToStackSlot(const MachineInstr &MI,
                              int &FrameIndex) const override;

  void copyPhysReg(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
                   const DebugLoc &DL, Register DestReg, Register SrcReg,
                   bool KillSrc, bool RenamableDest = false,
                   bool RenamableSrc = false) const override;

  void storeRegToStackSlot(
      MachineBasicBlock &MBB, MachineBasicBlock::iterator MI, Register SrcReg,
      bool isKill, int FrameIndex, const TargetRegisterClass *RC, Register VReg,
      MachineInstr::MIFlag Flags = MachineInstr::NoFlags) const override;

  void loadRegFromStackSlot(
      MachineBasicBlock &MBB, MachineBasicBlock::iterator MI, Register DestReg,
      int FrameIndex, const TargetRegisterClass *RC, Register VReg,
      unsigned SubReg = 0,
      MachineInstr::MIFlag Flags = MachineInstr::NoFlags) const override;

  bool analyzeBranch(MachineBasicBlock &MBB, MachineBasicBlock *&TBB,
                     MachineBasicBlock *&FBB,
                     SmallVectorImpl<MachineOperand> &Cond,
                     bool AllowModify = false) const override;

  unsigned removeBranch(MachineBasicBlock &MBB,
                        int *BytesRemoved = nullptr) const override;

  unsigned insertBranch(MachineBasicBlock &MBB, MachineBasicBlock *TBB,
                        MachineBasicBlock *FBB, ArrayRef<MachineOperand> Cond,
                        const DebugLoc &DL,
                        int *BytesAdded = nullptr) const override;

  bool
  reverseBranchCondition(SmallVectorImpl<MachineOperand> &Cond) const override;

  bool expandPostRAPseudo(MachineInstr &MI) const override;

  int getSPAdjust(const MachineInstr &MI) const override;

  unsigned getInstSizeInBytes(const MachineInstr &MI) const override;

  bool isBranchOffsetInRange(unsigned BranchOpc,
                             int64_t BrOffset) const override;

  MachineBasicBlock *getBranchDestBlock(const MachineInstr &MI) const override;

  void insertIndirectBranch(MachineBasicBlock &MBB,
                            MachineBasicBlock &NewDestBB,
                            MachineBasicBlock &RestoreBB, const DebugLoc &DL,
                            int64_t BrOffset = 0,
                            RegScavenger *RS = nullptr) const override;

  ArrayRef<std::pair<unsigned, const char *>>
  getSerializableDirectMachineOperandTargetFlags() const override;

private:
  const Z80Subtarget *STI;
};

namespace Z80 {

enum AddressSpace : unsigned { AS_Memory = 0, NumAddrSpaces };

/// Target-specific flags on symbol machine operands. They select which part
/// of the symbol's link-time address an 8-bit immediate slot receives; the
/// MC lowering wraps the flagged operand in the matching Z80MCExpr variant
/// so the existing Addr16_Low/High fixups and relocations carry it to the
/// linker.
enum TOF {
  MO_NO_FLAGS = 0,
  /// The low byte of the symbol's 16-bit address (sdasz80 "#<sym").
  MO_ADDR16_LO,
  /// The high byte of the symbol's 16-bit address (sdasz80 "#>sym").
  MO_ADDR16_HI,
};

} // namespace Z80

} // namespace llvm

#endif // not LLVM_LIB_TARGET_Z80_Z80INSTRINFO_H
