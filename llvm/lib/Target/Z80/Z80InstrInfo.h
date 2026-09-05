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
#include "Z80OpcodeUtils.h"
#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

#define GET_INSTRINFO_HEADER
#include "Z80GenInstrInfo.inc"

namespace llvm {

class Z80Subtarget;

namespace Z80 {
/// Which operation an ALU_A_FI performs. Stored as its first operand.
enum AluOp { ALU_ADD, ALU_SUB, ALU_AND, ALU_OR, ALU_XOR };

/// The IX-indexed form of an ALU operation.
inline unsigned getAluIXdOpcode(unsigned Op) {
  switch (Op) {
  case ALU_ADD:
    return Z80::ADD_A_IXd;
  case ALU_SUB:
    return Z80::SUB_IXd;
  case ALU_AND:
    return Z80::AND_IXd;
  case ALU_OR:
    return Z80::OR_IXd;
  case ALU_XOR:
    return Z80::XOR_IXd;
  }
  llvm_unreachable("unknown ALU operation");
}

/// The (HL) form of an ALU operation.
inline unsigned getAluHLindOpcode(unsigned Op) {
  switch (Op) {
  case ALU_ADD:
    return Z80::ADD_A_HLind;
  case ALU_SUB:
    return Z80::SUB_HLind;
  case ALU_AND:
    return Z80::AND_HLind;
  case ALU_OR:
    return Z80::OR_HLind;
  case ALU_XOR:
    return Z80::XOR_HLind;
  }
  llvm_unreachable("unknown ALU operation");
}

/// The register-operand pseudo form of an ALU operation.
inline unsigned getAluRegOpcode(unsigned Op) {
  switch (Op) {
  case ALU_ADD:
    return Z80::ADD_A_r;
  case ALU_SUB:
    return Z80::SUB_r;
  case ALU_AND:
    return Z80::AND_r;
  case ALU_OR:
    return Z80::OR_r;
  case ALU_XOR:
    return Z80::XOR_r;
  }
  llvm_unreachable("unknown ALU operation");
}

/// The IX-indexed form of an ALU-against-register opcode, or 0 when \p Opc
/// is not one of them.
inline unsigned getAluRegIXdOpcode(unsigned Opc) {
  switch (Opc) {
  case Z80::ADD_A_r:
    return Z80::ADD_A_IXd;
  case Z80::ADC_A_r:
    return Z80::ADC_A_IXd;
  case Z80::SUB_r:
    return Z80::SUB_IXd;
  case Z80::SBC_A_r:
    return Z80::SBC_A_IXd;
  case Z80::AND_r:
    return Z80::AND_IXd;
  case Z80::XOR_r:
    return Z80::XOR_IXd;
  case Z80::OR_r:
    return Z80::OR_IXd;
  case Z80::CP_r:
    return Z80::CP_IXd;
  default:
    return 0;
  }
}

// Builders for the instructions whose registers are operands. Each comes in
// two forms, one inserting before an iterator and one appending to a block,
// and each puts the operands in the order the instruction declares them —
// which is not always the order the assembly prints.

/// Emit `LD dst, src`, the 8-bit register copy. The registers are operands, so
/// they are named here rather than by choosing among as many opcodes as there
/// are pairs of registers.
inline MachineInstrBuilder buildLD8(MachineBasicBlock &MBB,
                                    MachineBasicBlock::iterator I,
                                    const DebugLoc &DL,
                                    const TargetInstrInfo &TII, MCRegister Dst,
                                    MCRegister Src) {
  return BuildMI(MBB, I, DL, TII.get(Z80::LD_r_r), Dst).addReg(Src);
}

/// Append `LD dst, src` to the end of \p MBB.
inline MachineInstrBuilder buildLD8(MachineBasicBlock *MBB, const DebugLoc &DL,
                                    const TargetInstrInfo &TII, MCRegister Dst,
                                    MCRegister Src) {
  return BuildMI(MBB, DL, TII.get(Z80::LD_r_r), Dst).addReg(Src);
}

/// Begin `LD dst, n`, the 8-bit immediate load. The caller supplies the
/// immediate, which may be a symbol reference as well as a constant.
inline MachineInstrBuilder
buildLD8n(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
          const DebugLoc &DL, const TargetInstrInfo &TII, MCRegister Dst) {
  return BuildMI(MBB, I, DL, TII.get(Z80::LD_r_n), Dst);
}

/// Append `LD dst, n` to the end of \p MBB.
inline MachineInstrBuilder buildLD8n(MachineBasicBlock *MBB, const DebugLoc &DL,
                                     const TargetInstrInfo &TII,
                                     MCRegister Dst) {
  return BuildMI(MBB, DL, TII.get(Z80::LD_r_n), Dst);
}

/// Emit an accumulator operation against \p Src. \p Opc is one of the ALU r
/// opcodes; A is implicit.
inline MachineInstrBuilder buildAlu8(MachineBasicBlock &MBB,
                                     MachineBasicBlock::iterator I,
                                     const DebugLoc &DL,
                                     const TargetInstrInfo &TII, unsigned Opc,
                                     MCRegister Src) {
  return BuildMI(MBB, I, DL, TII.get(Opc)).addReg(Src);
}

/// Append an accumulator operation against \p Src to the end of \p MBB.
inline MachineInstrBuilder buildAlu8(MachineBasicBlock *MBB, const DebugLoc &DL,
                                     const TargetInstrInfo &TII, unsigned Opc,
                                     MCRegister Src) {
  return BuildMI(MBB, DL, TII.get(Opc)).addReg(Src);
}

/// Emit `ADC HL, src` or `SBC HL, src`. \p Opc is Z80::ADC_HL_rr or
/// Z80::SBC_HL_rr; HL is both an input and the destination.
inline MachineInstrBuilder buildAdcSbcHL(MachineBasicBlock &MBB,
                                         MachineBasicBlock::iterator I,
                                         const DebugLoc &DL,
                                         const TargetInstrInfo &TII,
                                         unsigned Opc, MCRegister Src) {
  return BuildMI(MBB, I, DL, TII.get(Opc)).addReg(Src);
}

/// Append `ADC HL, src` or `SBC HL, src` to the end of \p MBB.
inline MachineInstrBuilder buildAdcSbcHL(MachineBasicBlock *MBB,
                                         const DebugLoc &DL,
                                         const TargetInstrInfo &TII,
                                         unsigned Opc, MCRegister Src) {
  return BuildMI(MBB, DL, TII.get(Opc)).addReg(Src);
}

/// Emit `ADD HL, src`. HL is both an input and the destination, so it does
/// not appear as an operand.
inline MachineInstrBuilder
buildAddHL(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
           const DebugLoc &DL, const TargetInstrInfo &TII, MCRegister Src) {
  return BuildMI(MBB, I, DL, TII.get(Z80::ADD_HL_rr)).addReg(Src);
}

/// Append `ADD HL, src` to the end of \p MBB.
inline MachineInstrBuilder buildAddHL(MachineBasicBlock *MBB,
                                      const DebugLoc &DL,
                                      const TargetInstrInfo &TII,
                                      MCRegister Src) {
  return BuildMI(MBB, DL, TII.get(Z80::ADD_HL_rr)).addReg(Src);
}

/// Begin `LD dst, nn`, the 16-bit immediate load. The caller supplies the
/// immediate, which may be a symbol reference as well as a constant.
inline MachineInstrBuilder
buildLD16n(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
           const DebugLoc &DL, const TargetInstrInfo &TII, MCRegister Dst) {
  return BuildMI(MBB, I, DL, TII.get(Z80::LD_rr_nn), Dst);
}

/// Emit `LD dst, (IX+d)` or `LD dst, (IY+d)`. \p Opc selects the index
/// register. The destination is named first, so the displacement is the
/// instruction's second operand.
inline MachineInstrBuilder
buildLoadIdx(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
             const DebugLoc &DL, const TargetInstrInfo &TII, unsigned Opc,
             MCRegister Dst, int64_t Off) {
  return BuildMI(MBB, I, DL, TII.get(Opc), Dst).addImm(Off);
}

/// Emit `LD (IX+d), src` or `LD (IY+d), src`. A store has no result, so the
/// displacement stays the first operand.
inline MachineInstrBuilder
buildStoreIdx(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
              const DebugLoc &DL, const TargetInstrInfo &TII, unsigned Opc,
              int64_t Off, MCRegister Src) {
  return BuildMI(MBB, I, DL, TII.get(Opc)).addImm(Off).addReg(Src);
}

/// The displacement of an IX- or IY-indexed access. A load names its
/// destination first, so the slot is not always the first operand.
inline const MachineOperand &idxSlotOperand(const MachineInstr &MI) {
  bool IsLoad =
      MI.getOpcode() == Z80::LD_r_IXd || MI.getOpcode() == Z80::LD_r_IYd;
  return MI.getOperand(IsLoad ? 1 : 0);
}

/// Emit `BIT b, src`, which sets the zero flag from one bit of a register.
inline MachineInstrBuilder buildBitTest(MachineBasicBlock &MBB,
                                        MachineBasicBlock::iterator I,
                                        const DebugLoc &DL,
                                        const TargetInstrInfo &TII,
                                        unsigned Bit, MCRegister Src) {
  return BuildMI(MBB, I, DL, TII.get(Z80::BIT_b_r)).addImm(Bit).addReg(Src);
}

/// Append `BIT b, src` to the end of \p MBB.
inline MachineInstrBuilder buildBitTest(MachineBasicBlock *MBB,
                                        const DebugLoc &DL,
                                        const TargetInstrInfo &TII,
                                        unsigned Bit, MCRegister Src) {
  return BuildMI(MBB, DL, TII.get(Z80::BIT_b_r)).addImm(Bit).addReg(Src);
}

/// Emit a CB-prefixed rotate or shift of \p Reg. \p Opc is one of the
/// `<op> r` opcodes; the register is both read and written.
inline MachineInstrBuilder buildRotate8(MachineBasicBlock &MBB,
                                        MachineBasicBlock::iterator I,
                                        const DebugLoc &DL,
                                        const TargetInstrInfo &TII,
                                        unsigned Opc, MCRegister Reg) {
  return BuildMI(MBB, I, DL, TII.get(Opc), Reg).addReg(Reg);
}

/// Append a CB-prefixed rotate or shift of \p Reg to the end of \p MBB.
inline MachineInstrBuilder buildRotate8(MachineBasicBlock *MBB,
                                        const DebugLoc &DL,
                                        const TargetInstrInfo &TII,
                                        unsigned Opc, MCRegister Reg) {
  return BuildMI(MBB, DL, TII.get(Opc), Reg).addReg(Reg);
}

/// Emit `LD dst, (HL)`, the byte load through HL.
inline MachineInstrBuilder
buildLoadHL(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
            const DebugLoc &DL, const TargetInstrInfo &TII, MCRegister Dst) {
  return BuildMI(MBB, I, DL, TII.get(Z80::LD_r_HLind), Dst);
}

/// Emit `LD (HL), src`, the byte store through HL.
inline MachineInstrBuilder
buildStoreHL(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
             const DebugLoc &DL, const TargetInstrInfo &TII, MCRegister Src) {
  return BuildMI(MBB, I, DL, TII.get(Z80::LD_HLind_r)).addReg(Src);
}

/// Emit `INC r` or `DEC r`. \p Opc is Z80::INC_r or Z80::DEC_r; the register
/// is both read and written, so it appears twice.
inline MachineInstrBuilder buildIncDec8(MachineBasicBlock &MBB,
                                        MachineBasicBlock::iterator I,
                                        const DebugLoc &DL,
                                        const TargetInstrInfo &TII,
                                        unsigned Opc, MCRegister Reg) {
  return BuildMI(MBB, I, DL, TII.get(Opc), Reg).addReg(Reg);
}

/// Append `INC r` or `DEC r` to the end of \p MBB.
inline MachineInstrBuilder buildIncDec8(MachineBasicBlock *MBB,
                                        const DebugLoc &DL,
                                        const TargetInstrInfo &TII,
                                        unsigned Opc, MCRegister Reg) {
  return BuildMI(MBB, DL, TII.get(Opc), Reg).addReg(Reg);
}

/// Emit `INC rr` or `DEC rr`. \p Opc is Z80::INC_rr or Z80::DEC_rr; the pair
/// is both read and written, so it appears twice.
inline MachineInstrBuilder buildIncDec16(MachineBasicBlock &MBB,
                                         MachineBasicBlock::iterator I,
                                         const DebugLoc &DL,
                                         const TargetInstrInfo &TII,
                                         unsigned Opc, MCRegister Pair) {
  return BuildMI(MBB, I, DL, TII.get(Opc), Pair).addReg(Pair);
}

/// Append `INC rr` or `DEC rr` to the end of \p MBB.
inline MachineInstrBuilder buildIncDec16(MachineBasicBlock *MBB,
                                         const DebugLoc &DL,
                                         const TargetInstrInfo &TII,
                                         unsigned Opc, MCRegister Pair) {
  return BuildMI(MBB, DL, TII.get(Opc), Pair).addReg(Pair);
}

/// Mark a register the instruction reads only incidentally as undef: the
/// result does not depend on the value (SBC A,A spreads carry whatever A
/// holds, AND A only clears carry, a flag-save PUSH AF only carries F), so
/// liveness must not demand a prior definition.
inline void markUndefUse(const MachineInstrBuilder &MIB, MCRegister Reg) {
  for (MachineOperand &MO : MIB.getInstr()->operands())
    if (MO.isReg() && MO.isUse() && MO.getReg() == Reg)
      MO.setIsUndef();
}

/// Emit `XOR A`, which sets A to zero whatever it held. The read of A is
/// incidental, so it is marked undef and does not keep a value alive.
inline MachineInstrBuilder buildZeroA(MachineBasicBlock &MBB,
                                      MachineBasicBlock::iterator I,
                                      const DebugLoc &DL,
                                      const TargetInstrInfo &TII) {
  auto MIB = buildAlu8(MBB, I, DL, TII, Z80::XOR_r, Z80::A);
  markUndefUse(MIB, Z80::A);
  return MIB;
}

/// Append `XOR A` to the end of \p MBB.
inline MachineInstrBuilder buildZeroA(MachineBasicBlock *MBB,
                                      const DebugLoc &DL,
                                      const TargetInstrInfo &TII) {
  auto MIB = buildAlu8(MBB, DL, TII, Z80::XOR_r, Z80::A);
  markUndefUse(MIB, Z80::A);
  return MIB;
}

/// Emit `SBC A,A`, which spreads the carry flag across every bit of A. Like
/// `XOR A` the read of A is incidental, so it is marked undef.
inline MachineInstrBuilder buildSbcAA(MachineBasicBlock &MBB,
                                      MachineBasicBlock::iterator I,
                                      const DebugLoc &DL,
                                      const TargetInstrInfo &TII) {
  auto MIB = buildAlu8(MBB, I, DL, TII, Z80::SBC_A_r, Z80::A);
  markUndefUse(MIB, Z80::A);
  return MIB;
}

/// Append `SBC A,A` to the end of \p MBB.
inline MachineInstrBuilder buildSbcAA(MachineBasicBlock *MBB,
                                      const DebugLoc &DL,
                                      const TargetInstrInfo &TII) {
  auto MIB = buildAlu8(MBB, DL, TII, Z80::SBC_A_r, Z80::A);
  markUndefUse(MIB, Z80::A);
  return MIB;
}

/// Whether \p Reg still carries a value where \p MI sits: the block receives it
/// live, or an earlier instruction defines it and nothing since has retired it.
/// This mirrors the bookkeeping the machine verifier does, so a read the scan
/// rejects is one that carries nothing.
inline bool hasLiveValue(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator MI, MCRegister Reg,
                         const TargetRegisterInfo *TRI) {
  for (MachineBasicBlock::iterator I = MI; I != MBB.begin();) {
    --I;
    bool LiveDef = false, Def = false, Use = false, Killed = false;
    for (const MachineOperand &MO : I->operands()) {
      if (!MO.isReg() || !MO.getReg().isPhysical() ||
          !TRI->regsOverlap(MO.getReg(), Reg))
        continue;
      if (MO.isDef()) {
        Def = true;
        LiveDef |= !MO.isDead();
      } else if (!MO.isUndef()) {
        Use = true;
        Killed |= MO.isKill();
      }
    }
    // Definitions take effect after the reads of the same instruction.
    if (Def)
      return LiveDef;
    if (Use)
      return !Killed;
  }
  for (const auto &LI : MBB.liveins())
    if (TRI->regsOverlap(LI.PhysReg, Reg))
      return true;
  return false;
}

/// Collect the halves among \p MI's pair reads that hold no value. A pair the
/// register allocator filled in one half only is still read whole, with no
/// operand recording which half is empty, so an expansion that splits the pair
/// into byte accesses has to settle the question again here. Only the halves
/// are in question: a pair with nothing in either half, like any other read of
/// a register that holds nothing, is already an error on \p MI itself.
inline void collectEmptyReadBytes(MachineBasicBlock &MBB,
                                  MachineBasicBlock::iterator MI,
                                  const TargetRegisterInfo *TRI,
                                  SmallVectorImpl<MCRegister> &Empty) {
  for (const MachineOperand &MO : MI->operands()) {
    if (!MO.isReg() || !MO.isUse() || MO.isUndef() || !MO.getReg().isPhysical())
      continue;
    auto Halves = TRI->subregs(MO.getReg().asMCReg());
    if (Halves.empty())
      continue;
    for (MCPhysReg Sub : Halves)
      if (!hasLiveValue(MBB, MI, Sub, TRI) &&
          !is_contained(Empty, MCRegister(Sub)))
        Empty.push_back(Sub);
  }
}

/// Mark every read of an empty byte in [\p Begin, \p End) undef, so that the
/// expansion does not claim to read a value that was never produced. A byte the
/// range itself writes carries a value from that point on and leaves the set.
inline void markEmptyReads(MachineBasicBlock::iterator Begin,
                           MachineBasicBlock::iterator End,
                           const TargetRegisterInfo *TRI,
                           SmallVectorImpl<MCRegister> &Empty) {
  for (auto It = Begin; It != End && !Empty.empty(); ++It) {
    for (MachineOperand &MO : It->operands())
      if (MO.isReg() && MO.isUse() && MO.getReg().isPhysical() &&
          is_contained(Empty, MO.getReg().asMCReg()))
        MO.setIsUndef();
    for (const MachineOperand &MO : It->operands())
      if (MO.isReg() && MO.isDef() && MO.getReg().isPhysical())
        llvm::erase_if(Empty, [&](MCRegister R) {
          return TRI->regsOverlap(MO.getReg(), R);
        });
  }
}

/// Collect the registers \p MI reads without a value, down to their halves. A
/// rewrite that re-expresses those reads has to say the same about them, since
/// nothing it emits gives the register a value either.
inline void collectUndefReads(const MachineInstr &MI,
                              const TargetRegisterInfo *TRI,
                              SmallVectorImpl<MCRegister> &Empty) {
  for (const MachineOperand &MO : MI.operands()) {
    if (!MO.isReg() || !MO.isUse() || !MO.isUndef() ||
        !MO.getReg().isPhysical())
      continue;
    for (MCPhysReg Sub : TRI->subregs_inclusive(MO.getReg().asMCReg()))
      if (!is_contained(Empty, MCRegister(Sub)))
        Empty.push_back(Sub);
  }
}

/// Whether \p MI reads \p Reg without a value. What such a store leaves in a
/// frame slot is nothing, so nothing may be taken from the register later on
/// the grounds that the slot holds what it holds. A register read more than
/// once counts as carrying a value if any of those reads does.
inline bool readsUndef(const MachineInstr &MI, Register Reg) {
  bool Found = false;
  for (const MachineOperand &MO : MI.operands()) {
    if (!MO.isReg() || !MO.isUse() || MO.getReg() != Reg)
      continue;
    if (!MO.isUndef())
      return false;
    Found = true;
  }
  return Found;
}

/// Whether \p Reg still matters where \p At sits: something at or below it
/// reads the value, or a successor expects it. This is the question behind
/// every save-and-restore decision and every peephole that borrows a register.
///
/// Reserved registers are asked about by membership: LivePhysRegs reports them
/// as unavailable whether or not they carry anything, so only that answer is
/// meaningful for FLAGS. For everything else availability is the right test,
/// since a pair matters as soon as either half does.
inline bool isLiveAt(MachineBasicBlock &MBB, MachineBasicBlock::iterator At,
                     MCRegister Reg, const TargetRegisterInfo *TRI) {
  const MachineRegisterInfo &MRI = MBB.getParent()->getRegInfo();
  LivePhysRegs Live(*TRI);
  Live.addLiveOutsNoPristines(MBB);
  for (MachineBasicBlock::iterator I = MBB.end(); I != At;) {
    --I;
    Live.stepBackward(*I);
  }
  if (MRI.isReserved(Reg))
    return Live.contains(Reg);
  return !Live.available(MRI, Reg);
}

/// Keep \p Reg readable at \p At. Register allocation marked the last read of
/// a value as a kill (or its only definition as dead); a read placed after
/// that point leaves the flag describing a live range that is now too short,
/// so clear the flags back to the definition the new read takes its value
/// from.
inline void extendLiveRangeTo(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator At, MCRegister Reg,
                              const TargetRegisterInfo *TRI) {
  for (MachineBasicBlock::iterator I = At; I != MBB.begin();) {
    --I;
    bool Def = false;
    for (MachineOperand &MO : I->operands()) {
      if (!MO.isReg() || !MO.getReg().isPhysical() ||
          !TRI->regsOverlap(MO.getReg(), Reg))
        continue;
      if (MO.isDef()) {
        MO.setIsDead(false);
        Def = true;
      } else {
        MO.setIsKill(false);
      }
    }
    if (Def)
      return;
  }
}

/// Emit the PUSH of a pair saved across an inserted sequence. The read is
/// placed after register allocation settled the pair's liveness, so a value it
/// still carries has a kill (or a dead definition) that now ends too early,
/// and a pair carrying nothing must say so rather than name a value that was
/// never produced.
inline void emitPairSavePush(MachineBasicBlock &MBB,
                             MachineBasicBlock::iterator At, const DebugLoc &DL,
                             const TargetInstrInfo &TII, MCRegister Pair) {
  const TargetRegisterInfo *TRI =
      MBB.getParent()->getSubtarget().getRegisterInfo();
  // Settle the question before the push exists, or the scan back from the
  // insertion point reads the push's own operand as the answer. Either half
  // carrying something is enough, since the save covers the whole pair.
  bool Carries = false;
  for (MCPhysReg Half : TRI->subregs(Pair))
    if (hasLiveValue(MBB, At, Half, TRI)) {
      Carries = true;
      break;
    }
  MachineInstrBuilder Push =
      BuildMI(MBB, At, DL, TII.get(Z80::getPushOpcode(Pair)));
  if (Carries)
    extendLiveRangeTo(MBB, Push->getIterator(), Pair, TRI);
  else
    markUndefUse(Push, Pair);
}

/// Emit the PUSH HL of a sequence that borrows HL, on the same terms.
inline void emitHLSavePush(MachineBasicBlock &MBB,
                           MachineBasicBlock::iterator At, const DebugLoc &DL,
                           const TargetInstrInfo &TII) {
  emitPairSavePush(MBB, At, DL, TII, Z80::HL);
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

  MachineInstr *foldMemoryOperandImpl(MachineFunction &MF, MachineInstr &MI,
                                      ArrayRef<unsigned> Ops, int FrameIndex,
                                      MachineInstr *&CopyMI,
                                      LiveIntervals *LIS = nullptr,
                                      VirtRegMap *VRM = nullptr) const override;

  unsigned getInstSizeInBytes(const MachineInstr &MI) const override;

  //===--------------------------------------------------------------------===//
  // Machine outliner
  //===--------------------------------------------------------------------===//

  bool shouldOutlineFromFunctionByDefault(MachineFunction &MF) const override;

  bool isFunctionSafeToOutlineFrom(MachineFunction &MF,
                                   bool OutlineFromLinkOnceODRs) const override;

  std::optional<std::unique_ptr<outliner::OutlinedFunction>>
  getOutliningCandidateInfo(
      const MachineModuleInfo &MMI,
      std::vector<outliner::Candidate> &RepeatedSequenceLocs,
      unsigned MinRepeats) const override;

  outliner::InstrType
  getOutliningTypeImpl(const MachineModuleInfo &MMI,
                       MachineBasicBlock::iterator &MIT,
                       unsigned Flags) const override;

  void buildOutlinedFrame(MachineBasicBlock &MBB, MachineFunction &MF,
                          const outliner::OutlinedFunction &OF) const override;

  MachineBasicBlock::iterator
  insertOutlinedCall(Module &M, MachineBasicBlock &MBB,
                     MachineBasicBlock::iterator &It, MachineFunction &MF,
                     outliner::Candidate &C) const override;

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
  /// The expansion itself. The public entry point wraps it to carry the
  /// pseudo's memory operands onto whatever performs the access.
  bool expandPostRAPseudoImpl(MachineInstr &MI) const;

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
