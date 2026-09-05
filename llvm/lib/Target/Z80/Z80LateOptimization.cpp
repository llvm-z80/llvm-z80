//===-- Z80LateOptimization.cpp - Z80 Late Optimization -------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the Z80 late optimization pass.
//
// This pass performs IX-indexed store-to-load forwarding after pseudo
// instructions have been expanded. When a value is spilled to the stack via
// LD (IX+d),R and later reloaded via LD R',(IX+d), this pass replaces the
// reload with a direct LD R',R (or eliminates it if R'==R).
//
//===----------------------------------------------------------------------===//

#include "Z80LateOptimization.h"

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80.h"
#include "Z80Subtarget.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "z80-late-opt"

using namespace llvm;

// Custom DenseMapInfo for IX offsets.  The default DenseMapInfo<int8_t> uses
// -1 and -2 as sentinel values, which collide with valid IX offsets.
// Using int as the key type with out-of-range sentinels avoids this.
struct IXOffsetInfo {
  static inline int getEmptyKey() { return 256; }
  static inline int getTombstoneKey() { return 257; }
  static unsigned getHashValue(int V) {
    return DenseMapInfo<int>::getHashValue(V);
  }
  static bool isEqual(int LHS, int RHS) { return LHS == RHS; }
};

// --- Asking an instruction what it is ---
//
// Most instructions here name their registers as operands, so a peephole
// matches on the opcode and the registers together rather than on one of the
// many opcodes a register pair used to have. These are the vocabulary for
// that; each returns an invalid register, or false, for anything else.

/// Whether \p MI is the 8-bit register copy.
static bool isLD8(const MachineInstr &MI) {
  return MI.getOpcode() == Z80::LD_r_r;
}

static bool isLD8(const MachineInstr &MI, Register Dst, Register Src) {
  return isLD8(MI) && MI.getOperand(0).getReg() == Dst &&
         MI.getOperand(1).getReg() == Src;
}

/// The source of a copy into \p Dst, or an invalid register if \p MI is not
/// a copy into that register.
static Register getLD8Src(const MachineInstr &MI, Register Dst) {
  if (!isLD8(MI) || MI.getOperand(0).getReg() != Dst)
    return Register();
  return MI.getOperand(1).getReg();
}

/// The destination of a copy out of \p Src, or an invalid register if \p MI
/// is not a copy out of that register.
static Register getLD8Dst(const MachineInstr &MI, Register Src) {
  if (!isLD8(MI) || MI.getOperand(1).getReg() != Src)
    return Register();
  return MI.getOperand(0).getReg();
}

/// Whether \p MI is the 8-bit immediate load.
static bool isLD8n(const MachineInstr &MI) {
  return MI.getOpcode() == Z80::LD_r_n;
}

/// The register an `LD r, n` writes, or an invalid register if \p MI is not
/// one. The immediate is its second operand, and can be a symbol reference
/// rather than a constant.
static Register getLD8nDst(const MachineInstr &MI) {
  return isLD8n(MI) ? MI.getOperand(0).getReg() : Register();
}

/// The pair an `LD rr, nn` writes, or an invalid register if \p MI is not
/// one. Like LD r,n the immediate is the second operand.
static Register getLD16nDst(const MachineInstr &MI) {
  return MI.getOpcode() == Z80::LD_rr_nn ? MI.getOperand(0).getReg()
                                         : Register();
}

/// Whether \p MI loads \p Dst from the byte HL points at.
static bool isLoadHL(const MachineInstr &MI, Register Dst) {
  return MI.getOpcode() == Z80::LD_r_HLind && MI.getOperand(0).getReg() == Dst;
}

/// Whether \p MI stores \p Src to the byte HL points at.
static bool isStoreHL(const MachineInstr &MI, Register Src) {
  return MI.getOpcode() == Z80::LD_HLind_r && MI.getOperand(0).getReg() == Src;
}

/// Whether \p R carries data rather than the address of an HL-indirect
/// access. H and L hold the address, and a rewrite that moved such an access
/// would be reasoning about the address as if it were data.
static bool isAccessDataReg(Register R) {
  return R.isValid() && R != Z80::H && R != Z80::L;
}

/// The register an `LD r, (HL)` loads, leaving out the halves of the address
/// itself, or an invalid register if \p MI is not such a load.
static Register getLoadHLindDstReg(const MachineInstr &MI) {
  if (MI.getOpcode() != Z80::LD_r_HLind)
    return Register();
  Register Dst = MI.getOperand(0).getReg();
  return isAccessDataReg(Dst) ? Dst : Register();
}

/// The register an `LD (HL), r` stores, leaving out the halves of the address
/// itself, or an invalid register if \p MI is not such a store.
static Register getStoreHLindSrcReg(const MachineInstr &MI) {
  if (MI.getOpcode() != Z80::LD_HLind_r)
    return Register();
  Register Src = MI.getOperand(0).getReg();
  return isAccessDataReg(Src) ? Src : Register();
}

/// The register an `LD (IX+d), r` stores, or an invalid register if \p MI is
/// not such a store.
static Register getStoreIXdSrcReg(const MachineInstr &MI) {
  return MI.getOpcode() == Z80::LD_IXd_r ? MI.getOperand(1).getReg()
                                         : Register();
}

/// The register an `LD r, (IX+d)` loads, or an invalid register if \p MI is
/// not such a load.
static Register getLoadIXdDstReg(const MachineInstr &MI) {
  return MI.getOpcode() == Z80::LD_r_IXd ? MI.getOperand(0).getReg()
                                         : Register();
}

/// Whether \p MI is the accumulator operation \p Opc against \p Src.
static bool isAlu8(const MachineInstr &MI, unsigned Opc, Register Src) {
  return MI.getOpcode() == Opc && MI.getOperand(0).getReg() == Src;
}

/// Whether \p MI is `XOR A`, which sets A to zero rather than combining it
/// with another register.
static bool isZeroA(const MachineInstr &MI) {
  return isAlu8(MI, Z80::XOR_r, Z80::A);
}

// Get the register an OR r / XOR r reads besides A, or Register(). Both
// leave A untouched when that register holds zero. A itself is excluded:
// OR A and XOR A mean something else.
static Register getZeroNeutralAluSrcReg(const MachineInstr &MI) {
  unsigned Opc = MI.getOpcode();
  if (Opc != Z80::OR_r && Opc != Z80::XOR_r)
    return Register();
  Register Src = MI.getOperand(0).getReg();
  return Src == Z80::A ? Register() : Src;
}

// Get the register an OR r / XOR r / ADD A,r reads besides A, or Register().
// All three copy that register into A when A holds zero.
static Register getAccumulatorNeutralAluSrcReg(const MachineInstr &MI) {
  unsigned Opc = MI.getOpcode();
  if (Opc != Z80::OR_r && Opc != Z80::XOR_r && Opc != Z80::ADD_A_r)
    return Register();
  Register Src = MI.getOperand(0).getReg();
  return Src == Z80::A ? Register() : Src;
}

// For an 8-bit ALU instruction that reads a register, give back that
// register and the IX-indexed form of the same operation. A is excluded:
// the folded form addresses memory, which A cannot stand in for.
static Register getAluRegSrc(const MachineInstr &MI, unsigned &IXdOpc) {
  unsigned Opc = Z80::getAluRegIXdOpcode(MI.getOpcode());
  if (!Opc)
    return Register();
  Register Src = MI.getOperand(0).getReg();
  if (Src == Z80::A)
    return Register();
  IXdOpc = Opc;
  return Src;
}

/// Whether \p MI increments or decrements \p Reg. \p Opc is Z80::INC_r or
/// Z80::DEC_r.
static bool isIncDec8(const MachineInstr &MI, unsigned Opc, Register Reg) {
  return MI.getOpcode() == Opc && MI.getOperand(0).getReg() == Reg;
}

/// Whether \p MI increments or decrements \p Pair. \p Opc is Z80::INC_rr or
/// Z80::DEC_rr.
static bool isIncDec16(const MachineInstr &MI, unsigned Opc, Register Pair) {
  return MI.getOpcode() == Opc && MI.getOperand(0).getReg() == Pair;
}

// Whether a stack access may be reasoned about as an ordinary read or write
// of a frame slot. The memory operand rides on the frame index pseudo from
// selection through expansion, so an access with none left is one this pass
// did not follow and should not draw conclusions from, and a volatile one is
// an access the program asked to actually perform.
static bool isPlainSlotAccess(const MachineInstr &MI) {
  return !MI.memoperands_empty() &&
         llvm::all_of(MI.memoperands(), [](const MachineMemOperand *MMO) {
           return MMO->isUnordered() && !MMO->isVolatile();
         });
}

// Invalidate all AvailValues entries where the stored register overlaps
// with the given clobbered register.
static void invalidateReg(DenseMap<int, MCPhysReg, IXOffsetInfo> &AvailValues,
                          const TargetRegisterInfo *TRI,
                          MCPhysReg ClobberedReg) {
  SmallVector<int, 4> ToErase;
  for (auto &KV : AvailValues) {
    if (TRI->regsOverlap(KV.second, ClobberedReg))
      ToErase.push_back(KV.first);
  }
  for (int K : ToErase)
    AvailValues.erase(K);
}

namespace {

class Z80LateOptimization : public MachineFunctionPass {
public:
  static char ID;

  Z80LateOptimization() : MachineFunctionPass(ID) {
    llvm::initializeZ80LateOptimizationPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
};

// --- SM83: reuse the address LDHL SP,e left in HL ---
//
// Every stack slot access recomputes its address from scratch, but spill
// code touches neighboring slots in bursts, so HL usually still holds an
// address one byte away (the 16-bit spill expansion even ends on slot+1
// via LD (HL+)). Track what HL holds relative to the current SP and turn
// a recomputation into nothing (same slot) or INC/DEC HL (next slot).
//
// The offset is tracked relative to SP, so SP movement (PUSH/POP/ADD SP)
// shifts it; a call ends tracking, since callee cleanup leaves SP
// unknowable here (the same reason the push ix/pop hl rewrite was
// abandoned). LDHL also defines FLAGS, so a rewrite needs FLAGS dead.
static bool reuseLDHLAddress(MachineBasicBlock &MBB,
                             const TargetInstrInfo *TII,
                             const TargetRegisterInfo *TRI);

// Whether nothing at or below \p After wants the value in \p Reg, which is
// what lets a transformation clobber it.
static bool isRegDeadAfter(MachineBasicBlock::iterator After,
                           MachineBasicBlock &MBB,
                           const TargetRegisterInfo *TRI, MCPhysReg Reg) {
  return !Z80::isLiveAt(MBB, After, Reg, TRI);
}

// OR or XOR against a register holding zero leaves A exactly as it was, so
// all such an instruction really does is set flags. Where those are dead
// too it does nothing at all. These come from wide values whose upper half
// is a known zero: the byte-wise expansion has no way to see it.
//
// Dropping the instruction usually leaves the constant that fed it dead as
// well, which the sweep below then takes.
static bool elideZeroOperandLogic(MachineBasicBlock &MBB,
                                  const TargetInstrInfo *TII,
                                  const TargetRegisterInfo *TRI) {
  static const MCPhysReg Regs8[] = {Z80::A, Z80::B, Z80::C, Z80::D,
                                    Z80::E, Z80::H, Z80::L};
  bool Changed = false;
  SmallSet<MCPhysReg, 8> Zero;

  for (auto MII = MBB.begin(); MII != MBB.end();) {
    MachineInstr &MI = *MII;

    if (Register Src = getZeroNeutralAluSrcReg(MI);
        Src.isValid() && Zero.count(Src.asMCReg()) &&
        isRegDeadAfter(std::next(MII), MBB, TRI, Z80::FLAGS)) {
      LLVM_DEBUG(dbgs() << "  Zero operand, no effect: " << MI);
      MII = MBB.erase(MII);
      Changed = true;
      continue;
    }

    // The mirror case: a zero accumulator makes OR, XOR and ADD A a plain
    // move. Written as one, the copy in and the copy back out cancel, and
    // the peepholes below take both.
    if (Register Src = getAccumulatorNeutralAluSrcReg(MI);
        Src.isValid() && Zero.count(Z80::A) &&
        isRegDeadAfter(std::next(MII), MBB, TRI, Z80::FLAGS)) {
      LLVM_DEBUG(dbgs() << "  Zero accumulator, is a move: " << MI);
      Z80::buildLD8(MBB, MII, MI.getDebugLoc(), *TII, Z80::A, Src);
      MII = MBB.erase(MII);
      Zero.erase(Z80::A);
      Changed = true;
      continue;
    }

    if (MI.isCall() || MI.isInlineAsm()) {
      Zero.clear();
    } else {
      for (const MachineOperand &MO : MI.operands())
        if (MO.isReg() && MO.isDef() && MO.getReg().isPhysical())
          for (MCPhysReg R : Regs8)
            if (TRI->regsOverlap(MO.getReg(), R))
              Zero.erase(R);

      if (isZeroA(MI))
        Zero.insert(Z80::A);
      if (Register Src = getLD8Src(MI, Z80::A);
          Src.isValid() && Src != Z80::A && Zero.count(Src))
        Zero.insert(Z80::A);
      Register Narrow = getLD8nDst(MI);
      Register Pair = getLD16nDst(MI);
      if (Narrow.isValid() || Pair.isValid()) {
        const MachineOperand &Val = MI.getOperand(1);
        if (Val.isImm() && Val.getImm() == 0) {
          if (Pair.isValid())
            for (MCSubRegIterator SR(Pair, TRI); SR.isValid(); ++SR)
              Zero.insert(*SR);
          else
            Zero.insert(Narrow.asMCReg());
        }
      }
    }
    ++MII;
  }
  if (Changed)
    recomputeLivenessFlags(MBB);
  return Changed;
}

// Ways a register ends up carrying a value between memory and somewhere it
// did not need to stop:
//
//   ld c,(ix+d) ; ld a,c       ->  ld a,(ix+d)
//   ld c,(hl)   ; ld a,c       ->  ld a,(hl)
//   ld c,#n     ; ld (ix+d),c  ->  ld (ix+d),#n
//
// Each costs a byte and occupies a register the allocator had no reason to
// spend. The register has to be dead afterwards, since it stops being
// written.
static bool foldCopyIntoFrameAccess(MachineBasicBlock &MBB,
                                    const TargetInstrInfo *TII,
                                    const TargetRegisterInfo *TRI) {
  bool Changed = false;
  for (auto MII = MBB.begin(); MII != MBB.end();) {
    auto Next = std::next(MII);
    if (Next == MBB.end()) {
      ++MII;
      continue;
    }
    const DebugLoc &DL = MII->getDebugLoc();

    bool FromSlot = getLoadIXdDstReg(*MII).isValid();
    Register Loaded =
        FromSlot ? getLoadIXdDstReg(*MII) : getLoadHLindDstReg(*MII);
    if (Loaded.isValid()) {
      int64_t Slot = FromSlot ? Z80::idxSlotOperand(*MII).getImm() : 0;
      // The consumer to fold into. It need not come next, as long as
      // nothing in between disturbs the register or what the load reads.
      auto Use = MBB.end();
      Register Copied;
      unsigned IXdOpc = 0;
      unsigned Budget = 32;
      for (auto Scan = Next; Scan != MBB.end() && Budget; ++Scan, --Budget) {
        if (FromSlot && getAluRegSrc(*Scan, IXdOpc) == Loaded) {
          Use = Scan;
          break;
        }
        Copied = getLD8Dst(*Scan, Loaded);
        if (Copied.isValid()) {
          Use = Scan;
          break;
        }
        if (Scan->isCall() || Scan->isInlineAsm() ||
            Scan->readsRegister(Loaded, TRI) ||
            Scan->modifiesRegister(Loaded, TRI))
          break;
        // The read moves to where the use is, so nothing may write what it
        // reads on the way. Another frame slot is provably a different
        // byte; anything else could be a pointer into this one.
        if (Scan->mayStore()) {
          bool OtherSlot = FromSlot &&
                           (getStoreIXdSrcReg(*Scan).isValid() ||
                            Scan->getOpcode() == Z80::LD_IXd_n) &&
                           Z80::idxSlotOperand(*Scan).getImm() != Slot;
          if (!OtherSlot)
            break;
        }
        // The load is the same load only while the register it addresses
        // through still holds the address.
        if (Scan->modifiesRegister(FromSlot ? Z80::IX : Z80::HL, TRI))
          break;
      }

      // A load through HL cannot be redirected into H or L: that is the
      // address register, and anything reading it afterwards would see the
      // loaded byte instead.
      bool ClobbersAddr = !FromSlot && (Copied == Z80::H || Copied == Z80::L);
      if (Use != MBB.end() && Copied != Loaded && !ClobbersAddr &&
          isRegDeadAfter(std::next(Use), MBB, TRI, Loaded.asMCReg())) {
        const DebugLoc &UseDL = Use->getDebugLoc();
        if (Copied.isValid()) {
          auto MIB = FromSlot ? Z80::buildLoadIdx(MBB, Use, UseDL, *TII,
                                                  Z80::LD_r_IXd, Copied, Slot)
                              : Z80::buildLoadHL(MBB, Use, UseDL, *TII, Copied);
          MIB.cloneMemRefs(*MII);
        } else {
          BuildMI(MBB, Use, UseDL, TII->get(IXdOpc))
              .addImm(Slot)
              .cloneMemRefs(*MII);
        }
        MBB.erase(Use);
        MII = MBB.erase(MII);
        Changed = true;
        continue;
      }
    }

    if (Register Held = getLD8nDst(*MII);
        Held.isValid() && getStoreIXdSrcReg(*Next) == Held &&
        MII->getOperand(1).isImm() &&
        isRegDeadAfter(std::next(Next), MBB, TRI, Held.asMCReg())) {
      BuildMI(MBB, MII, DL, TII->get(Z80::LD_IXd_n))
          .addImm(Next->getOperand(0).getImm())
          .addImm(MII->getOperand(1).getImm() & 0xFF)
          .cloneMemRefs(*Next);
      MII = MBB.erase(MII);
      MII = MBB.erase(MII);
      Changed = true;
      continue;
    }

    ++MII;
  }
  if (Changed)
    recomputeLivenessFlags(MBB);
  return Changed;
}

// A reload from a frame slot, or a constant loaded into a register, whose
// destination is overwritten before it is read buys nothing: a pair reloaded
// because one of its bytes is needed leaves the other one dead, and so does
// a constant whose only reader has just been dropped.
static bool eraseDeadFrameReloads(MachineBasicBlock &MBB,
                                  const TargetRegisterInfo *TRI) {
  bool Changed = false;
  for (auto MII = MBB.begin(); MII != MBB.end();) {
    Register Dst = getLoadIXdDstReg(*MII);
    bool IsSlotLoad = Dst.isValid();
    if (!Dst.isValid())
      Dst = getLD8nDst(*MII);
    if (!Dst.isValid() && isLD8(*MII))
      Dst = MII->getOperand(0).getReg();
    if (!Dst.isValid())
      Dst = getLD16nDst(*MII);
    if (!Dst.isValid() || (IsSlotLoad && !isPlainSlotAccess(*MII)) ||
        !isRegDeadAfter(std::next(MII), MBB, TRI, Dst)) {
      ++MII;
      continue;
    }
    LLVM_DEBUG(dbgs() << "  Removing dead def: " << *MII);
    MII = MBB.erase(MII);
    Changed = true;
  }
  if (Changed)
    recomputeLivenessFlags(MBB);
  return Changed;
}

// SM83 reloads a 16-bit frame slot through HL, which doubles as the address
// register: LD A,(HL+); LD H,(HL); LD L,A. When the value was wanted in
// another pair the copy out of HL follows, and the two halves can simply be
// loaded where they belong instead.
//
//   ld a,(hl+) ; ld h,(hl) ; ld l,a ; ld c,l ; ld b,h
//     ->  ld a,(hl+) ; ld b,(hl) ; ld c,a
//
// Only when HL itself is dead afterwards, since the rewrite stops writing it.
static bool reloadDirectlyIntoPair(MachineBasicBlock &MBB,
                                   const TargetInstrInfo *TII,
                                   const TargetRegisterInfo *TRI) {
  static const struct {
    MCPhysReg Lo, Hi;
  } Pairs[] = {{Z80::C, Z80::B}, {Z80::E, Z80::D}};

  bool Changed = false;
  for (auto MII = MBB.begin(); MII != MBB.end();) {
    auto Load = MII;
    if (Load->getOpcode() != Z80::LD_A_HLI) {
      ++MII;
      continue;
    }
    auto LoadHi = std::next(Load);
    if (LoadHi == MBB.end() || !isLoadHL(*LoadHi, Z80::H)) {
      ++MII;
      continue;
    }
    auto SetL = std::next(LoadHi);
    if (SetL == MBB.end() || !isLD8(*SetL, Z80::L, Z80::A)) {
      ++MII;
      continue;
    }
    auto SetLo = std::next(SetL);
    if (SetLo == MBB.end()) {
      ++MII;
      continue;
    }
    auto SetHi = std::next(SetLo);
    if (SetHi == MBB.end()) {
      ++MII;
      continue;
    }

    const auto *P = llvm::find_if(Pairs, [&](const auto &P) {
      return isLD8(*SetLo, P.Lo, Z80::L) && isLD8(*SetHi, P.Hi, Z80::H);
    });
    if (P == std::end(Pairs) ||
        !isRegDeadAfter(std::next(SetHi), MBB, TRI, Z80::HL)) {
      ++MII;
      continue;
    }

    LLVM_DEBUG(dbgs() << "  Reload straight into pair: " << *SetLo);
    Z80::buildLoadHL(MBB, LoadHi, LoadHi->getDebugLoc(), *TII, P->Hi)
        .cloneMemRefs(*LoadHi);
    Z80::buildLD8(MBB, LoadHi, LoadHi->getDebugLoc(), *TII, P->Lo, Z80::A);
    MII = std::next(SetHi);
    MBB.erase(LoadHi, MII);
    Changed = true;
  }
  if (Changed)
    recomputeLivenessFlags(MBB);
  return Changed;
}

// --- Increment a register where it lives ---
//
// LD A,r; INC/DEC A; LD r,A round-trips through A for a plain increment.
// INC r produces the same value and the same flags, in one byte. Valid on
// both targets; needs A dead afterward, since the round trip left the new
// value in A as a side effect.
static bool directIncDec(MachineBasicBlock &MBB, const TargetInstrInfo *TII,
                         const TargetRegisterInfo *TRI) {
  bool Changed = false;
  for (auto MII = MBB.begin(), MIE = MBB.end(); MII != MIE;) {
    auto Next = std::next(MII);
    Register R = getLD8Src(*MII, Z80::A);
    if (!R || Next == MIE) {
      MII = Next;
      continue;
    }
    bool IsInc = isIncDec8(*Next, Z80::INC_r, Z80::A);
    if (!IsInc && !isIncDec8(*Next, Z80::DEC_r, Z80::A)) {
      MII = Next;
      continue;
    }
    // A register the field cannot name has no INC r form to fold into. A
    // itself does, so a round trip through A of A is folded like any other;
    // it is only the index registers this turns away.
    if (!Z80::isEncodableGR8(R)) {
      MII = Next;
      continue;
    }
    auto Third = std::next(Next);
    if (Third == MIE || getLD8Dst(*Third, Z80::A) != R) {
      MII = Next;
      continue;
    }
    auto After = std::next(Third);
    if (!isRegDeadAfter(After, MBB, TRI, Z80::A)) {
      MII = Next;
      continue;
    }
    LLVM_DEBUG(dbgs() << "  Direct inc/dec: " << *MII);
    Z80::buildIncDec8(MBB, MII, MII->getDebugLoc(), *TII,
                      IsInc ? Z80::INC_r : Z80::DEC_r, R);
    MBB.erase(MII);
    MBB.erase(Next);
    MBB.erase(Third);
    MII = After;
    Changed = true;
  }
  if (Changed)
    recomputeLivenessFlags(MBB);
  return Changed;
}

// --- Keep a saved register on the stack across untouched stretches ---
//
// Consecutive stack accesses each save and restore a live HL around
// themselves, producing POP rr ... PUSH rr with nothing in between that
// cares. When the stretch touches neither rr nor SP (removing the pair
// leaves SP two lower there, so any SP-relative access would slip), the
// value can simply stay on the stack. The adjacent-pair case is handled
// by the POP/PUSH peephole above; this is its windowed extension.
static bool elidePopPushAcrossStretch(MachineBasicBlock &MBB,
                                      const TargetInstrInfo *TII,
                                      const TargetRegisterInfo *TRI) {
  static const struct {
    unsigned PopOpc;
    unsigned PushOpc;
    MCPhysReg Reg;
  } Pairs[] = {
      {Z80::POP_BC, Z80::PUSH_BC, Z80::BC},
      {Z80::POP_DE, Z80::PUSH_DE, Z80::DE},
      {Z80::POP_HL, Z80::PUSH_HL, Z80::HL},
  };
  bool Changed = false;
  for (auto MII = MBB.begin(), MIE = MBB.end(); MII != MIE;) {
    auto Next = std::next(MII);
    for (const auto &P : Pairs) {
      if (MII->getOpcode() != P.PopOpc)
        continue;
      unsigned Budget = 32;
      for (auto J = Next; J != MIE && Budget--; ++J) {
        if (J->getOpcode() == P.PushOpc) {
          // The pop also refills the register itself; anything reading it
          // after the push would see stale contents without the pair.
          if (!isRegDeadAfter(std::next(J), MBB, TRI, P.Reg))
            break;
          LLVM_DEBUG(dbgs() << "  Pop/push elision across stretch: " << *MII);
          MBB.erase(J);
          Next = MBB.erase(MII);
          Changed = true;
          break;
        }
        // Push and pop model their SP movement through getSPAdjust, not
        // operands, so ask both ways: anything that moves or even reads SP
        // would see it two bytes short inside the shortened stretch.
        if (J->isCall() || J->isBranch() || J->isTerminator() ||
            J->isInlineAsm() || J->readsRegister(P.Reg, TRI) ||
            J->modifiesRegister(P.Reg, TRI) || TII->getSPAdjust(*J) != 0 ||
            J->readsRegister(Z80::SP, TRI) ||
            J->modifiesRegister(Z80::SP, TRI))
          break;
      }
      break;
    }
    MII = Next;
  }
  if (Changed)
    recomputeLivenessFlags(MBB);
  return Changed;
}

// The mirror of the pass above: PUSH rr ... POP rr saves a pair across a
// stretch that never writes it, so the pair is doing nothing. What keeps the
// pass above from taking it is SP: everything the stretch reached through
// the stack was addressed with those two bytes already counted in. LDHL SP,e
// carries its own displacement, so taking two off each one puts the stretch
// back where it was.
//
// A displacement of less than two would have been addressing the saved value
// itself, which is not something the frame layout produces, and is refused
// rather than reasoned about.
static bool elidePushPopAcrossStretch(MachineBasicBlock &MBB,
                                      const TargetInstrInfo *TII,
                                      const TargetRegisterInfo *TRI) {
  static const struct {
    unsigned PushOpc;
    unsigned PopOpc;
    MCPhysReg Reg;
  } Pairs[] = {
      {Z80::PUSH_BC, Z80::POP_BC, Z80::BC},
      {Z80::PUSH_DE, Z80::POP_DE, Z80::DE},
      {Z80::PUSH_HL, Z80::POP_HL, Z80::HL},
  };
  bool Changed = false;
  for (auto MII = MBB.begin(), MIE = MBB.end(); MII != MIE;) {
    auto Next = std::next(MII);
    for (const auto &P : Pairs) {
      if (MII->getOpcode() != P.PushOpc)
        continue;
      SmallVector<MachineInstr *, 8> Rebase;
      unsigned Budget = 16;
      for (auto J = Next; J != MIE && Budget--; ++J) {
        // LDHL SP,e writes HL, so it is only rebasable when HL is not the
        // pair being saved: for that one, the pop is what puts it back.
        if (J->getOpcode() == Z80::LDHL_SP_e && P.Reg != Z80::HL) {
          if (SignExtend64<8>(J->getOperand(0).getImm()) < 2)
            break;
          Rebase.push_back(&*J);
          continue;
        }
        if (J->getOpcode() == P.PopOpc) {
          for (MachineInstr *MI : Rebase) {
            int64_t Disp = SignExtend64<8>(MI->getOperand(0).getImm()) - 2;
            MI->getOperand(0).setImm(Disp & 0xFF);
          }
          LLVM_DEBUG(dbgs() << "  Push/pop elision across stretch: " << *MII);
          MBB.erase(J);
          Next = MBB.erase(MII);
          Changed = true;
          break;
        }
        // The saved value only has to survive: a stretch that reads the pair
        // reads what it already holds. Push and pop model their SP movement
        // through getSPAdjust rather than operands, so ask both ways.
        if (J->isCall() || J->isBranch() || J->isTerminator() ||
            J->isInlineAsm() || J->modifiesRegister(P.Reg, TRI) ||
            TII->getSPAdjust(*J) != 0 || J->readsRegister(Z80::SP, TRI) ||
            J->modifiesRegister(Z80::SP, TRI))
          break;
      }
      break;
    }
    MII = Next;
  }
  if (Changed)
    recomputeLivenessFlags(MBB);
  return Changed;
}

// --- SM83: route (HL) accesses through A for the post-increment form ---
//
//   LD A,(HL); INC HL  -> LD A,(HL+)              (2B/4M -> 1B/2M)
//   LD (HL),A; INC HL  -> LD (HL+),A              (2B/4M -> 1B/2M)
//   LD r,(HL); INC HL  -> LD A,(HL+); LD r,A      (6M -> 5M, A dead)
//   LD (HL),r; INC HL  -> LD A,r; LD (HL+),A      (4M -> 3M, A dead)
//
// The non-A forms trade nothing in size and touch no flags; they only
// need A free to carry the value.
static bool fusePostIncAccess(MachineBasicBlock &MBB,
                              const TargetInstrInfo *TII,
                              const TargetRegisterInfo *TRI) {
  bool Changed = false;
  for (auto MII = MBB.begin(), MIE = MBB.end(); MII != MIE;) {
    MachineInstr &MI = *MII;
    auto Next = std::next(MII);
    if (Next == MIE || !isIncDec16(*Next, Z80::INC_rr, Z80::HL)) {
      MII = Next;
      continue;
    }
    auto After = std::next(Next);
    const DebugLoc &DL = MI.getDebugLoc();
    // A pair the register allocator filled in one half only reaches here as a
    // byte store of the empty half, and the trip through A re-expresses that
    // read without giving it a value.
    SmallVector<MCRegister, 4> Empty;
    Z80::collectUndefReads(MI, TRI, Empty);
    bool AtStart = MII == MBB.begin();
    auto Prev = AtStart ? MBB.end() : std::prev(MII);

    if (isLoadHL(MI, Z80::A)) {
      BuildMI(MBB, MII, DL, TII->get(Z80::LD_A_HLI));
    } else if (isStoreHL(MI, Z80::A)) {
      BuildMI(MBB, MII, DL, TII->get(Z80::LD_HLI_A));
    } else if (Register Dst = getLoadHLindDstReg(MI);
               Dst && isRegDeadAfter(After, MBB, TRI, Z80::A)) {
      BuildMI(MBB, MII, DL, TII->get(Z80::LD_A_HLI));
      Z80::buildLD8(MBB, MII, DL, *TII, Dst, Z80::A);
    } else if (Register Src = getStoreHLindSrcReg(MI);
               Src && Src != Z80::A &&
               isRegDeadAfter(After, MBB, TRI, Z80::A)) {
      Z80::buildLD8(MBB, MII, DL, *TII, Z80::A, Src);
      BuildMI(MBB, MII, DL, TII->get(Z80::LD_HLI_A));
    } else {
      MII = Next;
      continue;
    }
    LLVM_DEBUG(dbgs() << "  Post-inc fuse: " << MI);
    Z80::markEmptyReads(AtStart ? MBB.begin() : std::next(Prev), MII, TRI,
                        Empty);
    MBB.erase(MII);
    MBB.erase(Next);
    MII = After;
    Changed = true;
  }
  if (Changed)
    recomputeLivenessFlags(MBB);
  return Changed;
}

// --- SM83: buy constant stores a trip through A ---
//
// LD (HL),n costs two bytes per store and cannot post-increment; through A
// the store is one byte and fuses with a following INC HL into LD (HL+),A.
// Within a stretch where A carries nothing (no reads or writes), stores of
// one constant repeat often enough (array and struct initialization) that
// materializing the constant once pays for itself:
//   cost: XOR A = 1 byte (needs dead flags) or LD A,n = 2 bytes
//   gain: 1 byte per plain store, 2 bytes per store+INC HL pair
static bool materializeConstantStores(MachineBasicBlock &MBB,
                                      const TargetInstrInfo *TII,
                                      const TargetRegisterInfo *TRI) {
  bool Changed = false;

  auto WindowEnd = MBB.begin();
  while (WindowEnd != MBB.end()) {
    // Find the next maximal window with no genuine use or def of A.
    auto WindowBegin = WindowEnd;
    while (WindowBegin != MBB.end() &&
           (WindowBegin->isCall() || WindowBegin->isInlineAsm() ||
            WindowBegin->readsRegister(Z80::A, TRI) ||
            WindowBegin->modifiesRegister(Z80::A, TRI)))
      ++WindowBegin;
    WindowEnd = WindowBegin;
    SmallVector<MachineBasicBlock::iterator, 8> Stores;
    while (WindowEnd != MBB.end() && !WindowEnd->isCall() &&
           !WindowEnd->isInlineAsm() &&
           !WindowEnd->readsRegister(Z80::A, TRI) &&
           !WindowEnd->modifiesRegister(Z80::A, TRI)) {
      if (WindowEnd->getOpcode() == Z80::LD_HLind_n)
        Stores.push_back(WindowEnd);
      ++WindowEnd;
    }
    if (Stores.empty())
      continue;

    // The window not touching A is not enough: A may be carrying a value
    // straight through it to a reader beyond, which our constant would
    // clobber. Only proceed when A is dead past the window.
    if (!isRegDeadAfter(WindowEnd, MBB, TRI, Z80::A))
      continue;

    // Group the stores by constant and convert each group that profits.
    SmallVector<std::pair<int64_t, unsigned>, 4> Groups; // value, saving
    for (auto It : Stores) {
      int64_t V = It->getOperand(0).getImm() & 0xFF;
      bool Fused = std::next(It) != MBB.end() &&
                   isIncDec16(*std::next(It), Z80::INC_rr, Z80::HL);
      auto *G = llvm::find_if(Groups, [&](auto &P) { return P.first == V; });
      if (G == Groups.end())
        Groups.push_back({V, Fused ? 2u : 1u});
      else
        G->second += Fused ? 2 : 1;
    }
    // Convert only the best group: a second constant's LD A,n could land
    // between the first group's converted stores and corrupt what A holds.
    llvm::sort(Groups, [](auto &L, auto &R) { return L.second > R.second; });
    Groups.truncate(1);
    for (auto &G : Groups) {
      const int64_t Value = G.first;
      const unsigned Saving = G.second;
      auto FirstIt = *llvm::find_if(Stores, [&](auto It) {
        return (It->getOperand(0).getImm() & 0xFF) == Value;
      });
      bool FlagsDead = isRegDeadAfter(FirstIt, MBB, TRI, Z80::FLAGS);
      unsigned Cost = (Value == 0 && FlagsDead) ? 1 : 2;
      if (Saving <= Cost)
        continue;

      // Materialize the constant once, before its first store.
      const DebugLoc &DL = FirstIt->getDebugLoc();
      if (Value == 0 && FlagsDead)
        Z80::buildZeroA(MBB, FirstIt, DL, *TII);
      else
        Z80::buildLD8n(MBB, FirstIt, DL, *TII, Z80::A).addImm(Value);

      for (auto It : Stores) {
        if ((It->getOperand(0).getImm() & 0xFF) != Value)
          continue;
        auto NextIt = std::next(It);
        if (NextIt != MBB.end() && isIncDec16(*NextIt, Z80::INC_rr, Z80::HL)) {
          BuildMI(MBB, It, It->getDebugLoc(), TII->get(Z80::LD_HLI_A));
          MBB.erase(It);
          MBB.erase(NextIt);
        } else {
          Z80::buildStoreHL(MBB, It, It->getDebugLoc(), *TII, Z80::A);
          MBB.erase(It);
        }
      }
      Changed = true;
      LLVM_DEBUG(dbgs() << "  A invest: constant " << Value << " saves "
                        << (Saving - Cost) << "B\n");
    }
  }

  if (Changed)
    recomputeLivenessFlags(MBB);
  return Changed;
}

// The IX-indexed store of an immediate is four bytes, one more than the
// store of A. Investing in A once, where nothing else needs it, pays for
// itself from the third store of a value on, or the second when the value
// is zero and XOR A can produce it.
static bool materializeIXConstantStores(MachineBasicBlock &MBB,
                                        const TargetInstrInfo *TII,
                                        const TargetRegisterInfo *TRI) {
  bool Changed = false;

  auto WindowEnd = MBB.begin();
  while (WindowEnd != MBB.end()) {
    auto Touches = [&](MachineBasicBlock::iterator It) {
      return It->isCall() || It->isInlineAsm() ||
             It->readsRegister(Z80::A, TRI) || It->modifiesRegister(Z80::A, TRI);
    };
    auto WindowBegin = WindowEnd;
    while (WindowBegin != MBB.end() && Touches(WindowBegin))
      ++WindowBegin;
    WindowEnd = WindowBegin;
    SmallVector<MachineBasicBlock::iterator, 8> Stores;
    while (WindowEnd != MBB.end() && !Touches(WindowEnd)) {
      if (WindowEnd->getOpcode() == Z80::LD_IXd_n)
        Stores.push_back(WindowEnd);
      ++WindowEnd;
    }
    if (Stores.empty())
      continue;

    // A may be carrying a value straight through the window to a reader
    // beyond it, which the constant would destroy.
    if (!isRegDeadAfter(WindowEnd, MBB, TRI, Z80::A))
      continue;

    // Only the most repeated value: a second constant's LD A,n would land
    // between the first group's converted stores and corrupt what A holds.
    DenseMap<int64_t, unsigned> Counts;
    for (auto It : Stores)
      ++Counts[It->getOperand(1).getImm() & 0xFF];
    int64_t Value = 0;
    unsigned Saving = 0;
    for (auto &C : Counts)
      if (C.second > Saving) {
        Value = C.first;
        Saving = C.second;
      }

    auto FirstIt = *llvm::find_if(Stores, [&](auto It) {
      return (It->getOperand(1).getImm() & 0xFF) == Value;
    });
    bool FlagsDead = isRegDeadAfter(FirstIt, MBB, TRI, Z80::FLAGS);
    unsigned Cost = (Value == 0 && FlagsDead) ? 1 : 2;
    if (Saving <= Cost)
      continue;

    const DebugLoc &DL = FirstIt->getDebugLoc();
    if (Value == 0 && FlagsDead)
      Z80::buildZeroA(MBB, FirstIt, DL, *TII);
    else
      Z80::buildLD8n(MBB, FirstIt, DL, *TII, Z80::A).addImm(Value);

    for (auto It : Stores) {
      if ((It->getOperand(1).getImm() & 0xFF) != Value)
        continue;
      Z80::buildStoreIdx(MBB, It, It->getDebugLoc(), *TII, Z80::LD_IXd_r,
                         It->getOperand(0).getImm(), Z80::A);
      MBB.erase(It);
    }
    Changed = true;
    LLVM_DEBUG(dbgs() << "  A invest (IX): constant " << Value << " saves "
                      << (Saving - Cost) << "B\n");
  }

  if (Changed)
    recomputeLivenessFlags(MBB);
  return Changed;
}

static bool reuseLDHLAddress(MachineBasicBlock &MBB,
                             const TargetInstrInfo *TII,
                             const TargetRegisterInfo *TRI) {
  bool Changed = false;
  bool Known = false;  // Whether HL = SP + Off holds here.
  int64_t Off = 0;     // Relative to the CURRENT SP.
  bool AKnown = false; // Whether A holds the constant AVal here.
  int64_t AVal = 0;

  for (auto MII = MBB.begin(), MIE = MBB.end(); MII != MIE;) {
    MachineInstr &MI = *MII;
    unsigned Opc = MI.getOpcode();
    auto Next = std::next(MII);

    // --- A-constant reuse: a store of a constant A already holds can go
    // through A (half the size), and with a following INC HL it fuses
    // into the post-increment store. Reloading the same constant into A
    // is dropped outright.
    if (Opc == Z80::LD_HLind_n && AKnown &&
        (MI.getOperand(0).getImm() & 0xFF) == (AVal & 0xFF)) {
      if (Next != MIE && isIncDec16(*Next, Z80::INC_rr, Z80::HL)) {
        LLVM_DEBUG(dbgs() << "  A reuse: fusing to (hl+) " << MI);
        BuildMI(MBB, MII, MI.getDebugLoc(), TII->get(Z80::LD_HLI_A));
        auto AfterInc = std::next(Next);
        MBB.erase(MII);
        MBB.erase(Next);
        MII = AfterInc;
        Off += 1; // LD (HL+),A moved HL exactly as the INC HL did.
        Changed = true;
        continue;
      }
      LLVM_DEBUG(dbgs() << "  A reuse: store via A " << MI);
      Z80::buildStoreHL(MBB, MII, MI.getDebugLoc(), *TII, Z80::A);
      MII = MBB.erase(MII);
      Changed = true;
      continue;
    }
    if (getLD8nDst(MI) == Z80::A && AKnown && MI.getOperand(1).isImm() &&
        (MI.getOperand(1).getImm() & 0xFF) == (AVal & 0xFF)) {
      // LD A,n leaves flags alone, so the reload can simply go.
      LLVM_DEBUG(dbgs() << "  A reuse: erasing reload " << MI);
      MII = MBB.erase(MII);
      Changed = true;
      continue;
    }
    if (isZeroA(MI) && AKnown && (AVal & 0xFF) == 0 &&
        isRegDeadAfter(Next, MBB, TRI, Z80::FLAGS)) {
      LLVM_DEBUG(dbgs() << "  A reuse: erasing xor a " << MI);
      MII = MBB.erase(MII);
      Changed = true;
      continue;
    }

    // Track what A holds. LD A,n can carry a link-time symbol byte instead
    // of an immediate; its value is unknown here, so it falls through to
    // the modifiesRegister case and invalidates the tracking.
    if (getLD8nDst(MI) == Z80::A && MI.getOperand(1).isImm()) {
      AKnown = true;
      AVal = MI.getOperand(1).getImm() & 0xFF;
    } else if (isZeroA(MI)) {
      AKnown = true;
      AVal = 0;
    } else if (MI.isCall() || MI.isInlineAsm() ||
               MI.modifiesRegister(Z80::A, TRI)) {
      AKnown = false;
    }

    if (Opc == Z80::LDHL_SP_e) {
      // The stored immediate is the masked byte of a signed displacement.
      int64_t N = SignExtend64<8>(MI.getOperand(0).getImm() & 0xFF);
      if (Known && isRegDeadAfter(Next, MBB, TRI, Z80::FLAGS)) {
        int64_t D = N - Off;
        if (D == 0) {
          LLVM_DEBUG(dbgs() << "  LDHL reuse: erasing " << MI);
          MII = MBB.erase(MII);
          Changed = true;
          continue;
        }
        if (D == 1 || D == -1) {
          LLVM_DEBUG(dbgs() << "  LDHL reuse: inc/dec for " << MI);
          Z80::buildIncDec16(MBB, MII, MI.getDebugLoc(), *TII,
                             D == 1 ? Z80::INC_rr : Z80::DEC_rr, Z80::HL);
          MII = MBB.erase(MII);
          Off = N;
          Changed = true;
          continue;
        }
      }
      Known = true;
      Off = N;
      MII = Next;
      continue;
    }

    if (isIncDec16(MI, Z80::INC_rr, Z80::HL)) {
      Off += 1;
      MII = Next;
      continue;
    }
    if (isIncDec16(MI, Z80::DEC_rr, Z80::HL)) {
      Off -= 1;
      MII = Next;
      continue;
    }

    switch (Opc) {
    case Z80::LD_HLI_A:
    case Z80::LD_A_HLI:
      Off += 1;
      break;
    case Z80::LD_HLD_A:
    case Z80::LD_A_HLD:
      Off -= 1;
      break;
    // HL is pinned to an absolute address, so SP movement shifts the
    // SP-relative offset in the opposite direction.
    case Z80::PUSH_AF:
    case Z80::PUSH_BC:
    case Z80::PUSH_DE:
    case Z80::PUSH_HL:
      Off += 2;
      break;
    case Z80::POP_AF:
    case Z80::POP_BC:
    case Z80::POP_DE:
      Off -= 2;
      break;
    case Z80::INC_SP:
      Off -= 1;
      break;
    case Z80::DEC_SP:
      Off += 1;
      break;
    case Z80::ADD_SP_e:
      Off -= SignExtend64<8>(MI.getOperand(0).getImm() & 0xFF);
      break;
    case Z80::LD_SP_HL: // SP := HL, so HL = SP + 0.
      Off = 0;
      break;
    default:
      // POP_HL loads an unknown value; calls leave SP itself unknowable
      // (callee cleanup); anything else touching HL or SP ends tracking.
      if (MI.isCall() || MI.isInlineAsm() ||
          MI.modifiesRegister(Z80::HL, TRI) ||
          MI.modifiesRegister(Z80::SP, TRI))
        Known = false;
      break;
    }
    MII = Next;
  }

  if (Changed)
    recomputeLivenessFlags(MBB);
  return Changed;
}

bool Z80LateOptimization::runOnMachineFunction(MachineFunction &MF) {
  const auto &STI = MF.getSubtarget<Z80Subtarget>();
  const auto *TII = STI.getInstrInfo();
  const auto *TRI = STI.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    // The peepholes written inline below move reads past the point where
    // register allocation recorded a value as dying, which leaves the kill and
    // dead flags describing a live range that ended too early. The peepholes
    // kept in their own functions each restate the flags before returning;
    // these share the one call at the end of the block.
    bool BlockChanged = false;

    // Dropping a no-op is what leaves the constant behind it dead, so this
    // comes before the sweep.
    Changed |= elideZeroOperandLogic(MBB, TII, TRI);
    // Before the rest: they all reason about what a register holds, and a
    // definition nobody reads only muddies that.
    Changed |= eraseDeadFrameReloads(MBB, TRI);
    Changed |= foldCopyIntoFrameAccess(MBB, TII, TRI);

    // --- Peephole: POP rr; PUSH rr → (remove both) ---
    // When a register pair is popped and immediately pushed back, the stack
    // state is unchanged (SP net effect = 0, same value on stack). If the
    // register pair is dead after the push (overwritten before next use),
    // both instructions are redundant. Common on SM83 where consecutive
    // stack accesses via LDHL SP,# each need push/pop HL around them.
    for (MachineBasicBlock::iterator MII = MBB.begin(), MIE = MBB.end();
         MII != MIE;) {
      static const struct {
        unsigned PopOpc;
        unsigned PushOpc;
        MCPhysReg Reg;
      } PopPushPairs[] = {
          {Z80::POP_BC, Z80::PUSH_BC, Z80::BC},
          {Z80::POP_DE, Z80::PUSH_DE, Z80::DE},
          {Z80::POP_HL, Z80::PUSH_HL, Z80::HL},
      };

      unsigned Opc = MII->getOpcode();
      bool Matched = false;
      for (const auto &PP : PopPushPairs) {
        if (Opc != PP.PopOpc)
          continue;
        auto NextIt = std::next(MII);
        if (NextIt == MIE || NextIt->getOpcode() != PP.PushOpc)
          break;
        auto AfterPush = std::next(NextIt);
        if (!isRegDeadAfter(AfterPush, MBB, TRI, PP.Reg))
          break;
        LLVM_DEBUG(dbgs() << "  Removing redundant POP+PUSH: " << *MII);
        NextIt->eraseFromParent();
        MII = MBB.erase(MII);
        Changed = BlockChanged = true;
        Matched = true;
        break;
      }
      if (!Matched)
        ++MII;
    }

    // --- Peephole: LD A,r; DEC A; LD r,A; OR A; JR NZ → DEC r; JR NZ ---
    // Replaces a 5-instruction decrement-and-branch sequence (28T, 6B) with
    // DEC r; JR NZ (14T, 3B). DEC r sets Z flag correctly for JR NZ, and
    // stays within the analyzable branch framework. Works on Z80 and SM83.
    for (MachineBasicBlock::iterator MII = MBB.begin(), MIE = MBB.end();
         MII != MIE;) {
      // Match: LD A,r (identify counter register r)
      Register CounterReg = getLD8Src(*MII, Z80::A);
      if (!CounterReg.isValid() || CounterReg == Z80::A ||
          !Z80::isEncodableGR8(CounterReg)) {
        ++MII;
        continue;
      }
      auto I1 = MII;
      auto I2 = std::next(I1);
      if (I2 == MIE) {
        ++MII;
        continue;
      }
      auto I3 = std::next(I2);
      if (I3 == MIE) {
        ++MII;
        continue;
      }
      auto I4 = std::next(I3);
      if (I4 == MIE) {
        ++MII;
        continue;
      }
      auto I5 = std::next(I4);
      if (I5 == MIE) {
        ++MII;
        continue;
      }

      // Match: DEC A; LD r,A; OR A; JR NZ,target
      if (!isIncDec8(*I2, Z80::DEC_r, Z80::A) ||
          !isLD8(*I3, CounterReg, Z80::A) || !isAlu8(*I4, Z80::OR_r, Z80::A) ||
          I5->getOpcode() != Z80::JR_NZ_e) {
        ++MII;
        continue;
      }

      // The original sequence leaves A = r-1. The replacement doesn't
      // touch A, so we must verify A is dead after the sequence.
      if (!isRegDeadAfter(std::next(I5), MBB, TRI, Z80::A)) {
        ++MII;
        continue;
      }

      MachineBasicBlock *TargetMBB = I5->getOperand(0).getMBB();
      DebugLoc DL = I1->getDebugLoc();

      LLVM_DEBUG(dbgs() << "  Loop counter peephole: LD A,"
                        << printReg(CounterReg, TRI) << " sequence → DEC "
                        << printReg(CounterReg, TRI) << "; JR NZ\n");
      I5->eraseFromParent();
      I4->eraseFromParent();
      I3->eraseFromParent();
      I2->eraseFromParent();
      MII = MBB.erase(I1);
      Z80::buildIncDec8(MBB, MII, DL, *TII, Z80::DEC_r, CounterReg);
      BuildMI(MBB, MII, DL, TII->get(Z80::JR_NZ_e)).addMBB(TargetMBB);
      Changed = BlockChanged = true;
    }

    // --- Peephole: XOR #0xFF → CPL ---
    // CPL (1 byte) is equivalent to XOR #0xFF (2 bytes) for the A register
    // value, but sets flags differently (CPL: H=1,N=1, others unchanged;
    // XOR: S,Z,P from result, H=1,N=0,C=0). Safe only when FLAGS is dead.
    for (MachineBasicBlock::iterator MII = MBB.begin(), MIE = MBB.end();
         MII != MIE;) {
      MachineInstr &MI = *MII;
      if (MI.getOpcode() == Z80::XOR_n && MI.getOperand(0).getImm() == 0xFF) {
        auto After = std::next(MII);
        if (isRegDeadAfter(After, MBB, TRI, Z80::FLAGS)) {
          LLVM_DEBUG(dbgs() << "  XOR #0xFF → CPL: " << MI);
          BuildMI(MBB, MI, MI.getDebugLoc(), TII->get(Z80::CPL));
          MII = MBB.erase(MII);
          Changed = BlockChanged = true;
          continue;
        }
      }
      ++MII;
    }

    // --- Peephole: LD A,#0 → XOR A ---
    // XOR A (1 byte) sets A to 0 just like LD A,#0 (2 bytes), but also
    // sets FLAGS (Z=1, S=0, H=0, P=1, N=0, C=0). Safe when FLAGS is dead.
    for (MachineBasicBlock::iterator MII = MBB.begin(), MIE = MBB.end();
         MII != MIE;) {
      MachineInstr &MI = *MII;
      if (getLD8nDst(MI) == Z80::A && MI.getOperand(1).isImm() &&
          MI.getOperand(1).getImm() == 0) {
        auto After = std::next(MII);
        if (isRegDeadAfter(After, MBB, TRI, Z80::FLAGS)) {
          LLVM_DEBUG(dbgs() << "  LD A,#0 → XOR A: " << MI);
          Z80::buildZeroA(MBB, MI, MI.getDebugLoc(), *TII);
          MII = MBB.erase(MII);
          Changed = BlockChanged = true;
          continue;
        }
      }
      ++MII;
    }

    // --- Peephole: ALU #imm; ALU #imm → ALU #imm ---
    // When the same immediate ALU instruction appears consecutively, the
    // second is redundant for idempotent operations (AND, OR).
    // Most common case: AND #1; AND #1 after SBC A,A; AND #1 sequences.
    for (MachineBasicBlock::iterator MII = MBB.begin(), MIE = MBB.end();
         MII != MIE;) {
      MachineInstr &MI = *MII;
      auto NextIt = std::next(MII);
      if (NextIt != MIE && MI.getOpcode() == NextIt->getOpcode() &&
          (MI.getOpcode() == Z80::AND_n || MI.getOpcode() == Z80::OR_n) &&
          MI.getOperand(0).getImm() == NextIt->getOperand(0).getImm()) {
        LLVM_DEBUG(dbgs() << "  Removing redundant: " << *NextIt);
        NextIt->eraseFromParent();
        Changed = BlockChanged = true;
        continue;
      }
      ++MII;
    }

    // --- Peephole: LD rr,#imm; LDHL SP,#; LD (HL),lo; INC HL; LD (HL),hi
    //             → LDHL SP,#; LD (HL),#lo; INC HL; LD (HL),#hi (SM83 only) ---
    // When a 16-bit constant is stored to the stack via a register pair,
    // replace with immediate stores to (HL). Saves 1 byte (8B → 7B) per
    // occurrence and frees the register pair.
    if (STI.hasSM83()) {
      for (MachineBasicBlock::iterator MII = MBB.begin(), MIE = MBB.end();
           MII != MIE;) {
        MachineInstr &MI = *MII;
        Register Pair = getLD16nDst(MI);
        bool IsBC = Pair == Z80::BC;
        bool IsDE = Pair == Z80::DE;
        if (!IsBC && !IsDE) {
          ++MII;
          continue;
        }
        if (!MI.getOperand(1).isImm()) {
          ++MII;
          continue;
        }

        // Match 5 consecutive instructions.
        auto I2 = std::next(MII);
        if (I2 == MIE || I2->getOpcode() != Z80::LDHL_SP_e) {
          ++MII;
          continue;
        }
        auto I3 = std::next(I2);
        if (I3 == MIE) {
          ++MII;
          continue;
        }
        if (!isStoreHL(*I3, IsBC ? Z80::C : Z80::E)) {
          ++MII;
          continue;
        }

        auto I4 = std::next(I3);
        if (I4 == MIE || !isIncDec16(*I4, Z80::INC_rr, Z80::HL)) {
          ++MII;
          continue;
        }
        auto I5 = std::next(I4);
        if (I5 == MIE) {
          ++MII;
          continue;
        }
        if (!isStoreHL(*I5, IsBC ? Z80::B : Z80::D)) {
          ++MII;
          continue;
        }

        // Register pair must be dead after the store sequence.
        MCPhysReg PairReg = IsBC ? Z80::BC : Z80::DE;
        if (!isRegDeadAfter(std::next(I5), MBB, TRI, PairReg)) {
          ++MII;
          continue;
        }

        int64_t Imm = MI.getOperand(1).getImm();
        LLVM_DEBUG(dbgs() << "  Folding 16-bit const store: " << MI);

        // Replace LD (HL),lo → LD (HL),#imm_lo
        BuildMI(MBB, *I3, I3->getDebugLoc(), TII->get(Z80::LD_HLind_n))
            .addImm(Imm & 0xFF);
        I3->eraseFromParent();

        // Replace LD (HL),hi → LD (HL),#imm_hi
        BuildMI(MBB, *I5, I5->getDebugLoc(), TII->get(Z80::LD_HLind_n))
            .addImm((Imm >> 8) & 0xFF);
        I5->eraseFromParent();

        // Remove LD rr,#imm
        MII = MBB.erase(MII);
        Changed = BlockChanged = true;
      }
    }

    // --- Peephole: consecutive LDHL SP,#N → INC/DEC HL (SM83 only) ---
    // When two LDHL SP,# instructions target adjacent offsets with only
    // non-HL-modifying instructions between them, replace the second LDHL
    // with INC HL or DEC HL. Saves 1 byte (2B → 1B) per occurrence.
    // Common in consecutive byte-at-a-time stack initialization.
    if (STI.hasSM83()) {
      for (MachineBasicBlock::iterator MII = MBB.begin(), MIE = MBB.end();
           MII != MIE; ++MII) {
        if (MII->getOpcode() != Z80::LDHL_SP_e)
          continue;
        if (!MII->getOperand(0).isImm())
          continue;
        int64_t Offset1 = MII->getOperand(0).getImm();

        // Scan forward to find the next LDHL SP,#. Bail if any
        // intervening instruction modifies HL, modifies SP, or has
        // unmodeled side effects. SP changes must be caught explicitly
        // because PUSH/POP don't declare SP in their Defs.
        auto It = std::next(MII);
        bool Clobbered = false;
        while (It != MIE && It->getOpcode() != Z80::LDHL_SP_e) {
          // PUSH/POP modify SP but don't declare it as Def.
          if (It->isCall() || It->isReturn() || It->hasUnmodeledSideEffects() ||
              It->getOpcode() == Z80::PUSH_BC ||
              It->getOpcode() == Z80::PUSH_DE ||
              It->getOpcode() == Z80::PUSH_HL ||
              It->getOpcode() == Z80::PUSH_AF ||
              It->getOpcode() == Z80::POP_BC ||
              It->getOpcode() == Z80::POP_DE ||
              It->getOpcode() == Z80::POP_AF ||
              It->getOpcode() == Z80::ADD_SP_e) {
            // POP_HL also modifies HL, but we catch it via Defs below.
            Clobbered = true;
            break;
          }
          // Check explicit and implicit defs for HL and SP.
          for (const MachineOperand &MO : It->operands()) {
            if (MO.isReg() && MO.isDef() && MO.getReg().isPhysical() &&
                (TRI->regsOverlap(MO.getReg(), Z80::HL) ||
                 TRI->regsOverlap(MO.getReg(), Z80::SP))) {
              Clobbered = true;
              break;
            }
          }
          if (Clobbered)
            break;
          for (MCPhysReg Def : TII->get(It->getOpcode()).implicit_defs()) {
            if (TRI->regsOverlap(Def, Z80::HL) ||
                TRI->regsOverlap(Def, Z80::SP)) {
              Clobbered = true;
              break;
            }
          }
          if (Clobbered)
            break;
          ++It;
        }
        if (Clobbered || It == MIE)
          continue;
        if (It->getOpcode() != Z80::LDHL_SP_e || !It->getOperand(0).isImm())
          continue;

        int64_t Offset2 = It->getOperand(0).getImm();
        int64_t Diff = Offset2 - Offset1;
        if (Diff != 1 && Diff != -1)
          continue;
        // LDHL sets FLAGS (H,C), INC/DEC HL does not. Verify FLAGS is dead.
        if (!isRegDeadAfter(std::next(It), MBB, TRI, Z80::FLAGS))
          continue;

        LLVM_DEBUG(dbgs() << "  LDHL SP,#" << Offset2 << " → "
                          << (Diff == 1 ? "INC" : "DEC") << " HL\n");
        Z80::buildIncDec16(MBB, *It, It->getDebugLoc(), *TII,
                           Diff == 1 ? Z80::INC_rr : Z80::DEC_rr, Z80::HL);
        It->eraseFromParent();
        Changed = BlockChanged = true;
      }
    }

    // --- Peephole: fold constant into XOR compare (CMP_Z16 + imm) ---
    // When a XOR-based 16-bit compare uses a constant loaded into a register
    // pair, fold the constant into XOR immediate instructions.
    // LD rr,#imm; LD A,X; XOR rhi; LD B,A; LD A,Y; XOR rlo; OR B
    // → LD A,X; XOR #hi; LD B,A; LD A,Y; XOR #lo; OR B
    // Saves 1 byte (9B → 8B) per occurrence and frees the register pair.
    // Applies to both Z80 (XOR_CMP_Z16 for i32/i64) and SM83 (SM83_CMP_Z16).
    {
      for (MachineBasicBlock::iterator MII = MBB.begin(), MIE = MBB.end();
           MII != MIE;) {
        MachineInstr &MI = *MII;
        Register Pair = getLD16nDst(MI);
        bool IsBC = Pair == Z80::BC;
        bool IsDE = Pair == Z80::DE;
        if (!IsBC && !IsDE) {
          ++MII;
          continue;
        }
        if (!MI.getOperand(1).isImm()) {
          ++MII;
          continue;
        }

        // Match 7 consecutive instructions.
        auto I2 = std::next(MII);
        if (I2 == MIE) {
          ++MII;
          continue;
        }
        // I2: LD A,X (load high byte of compared value)
        Register I2Src = getLD8Src(*I2, Z80::A);
        if (!I2Src.isValid() && !isLoadHL(*I2, Z80::A)) {
          ++MII;
          continue;
        }

        auto I3 = std::next(I2);
        if (I3 == MIE) {
          ++MII;
          continue;
        }
        if (!isAlu8(*I3, Z80::XOR_r, IsBC ? Z80::B : Z80::D)) {
          ++MII;
          continue;
        }

        auto I4 = std::next(I3);
        if (I4 == MIE || !isLD8(*I4, Z80::B, Z80::A)) {
          ++MII;
          continue;
        }

        auto I5 = std::next(I4);
        if (I5 == MIE) {
          ++MII;
          continue;
        }
        // I5: LD A,Y (load low byte of compared value)
        Register I5Src = getLD8Src(*I5, Z80::A);
        if (!I5Src.isValid() && !isLoadHL(*I5, Z80::A)) {
          ++MII;
          continue;
        }

        auto I6 = std::next(I5);
        if (I6 == MIE) {
          ++MII;
          continue;
        }
        if (!isAlu8(*I6, Z80::XOR_r, IsBC ? Z80::C : Z80::E)) {
          ++MII;
          continue;
        }

        auto I7 = std::next(I6);
        if (I7 == MIE || !isAlu8(*I7, Z80::OR_r, Z80::B)) {
          ++MII;
          continue;
        }

        // Ensure lhs registers don't overlap with the constant pair.
        MCPhysReg PairReg = IsBC ? Z80::BC : Z80::DE;
        if (I2Src.isValid() && TRI->regsOverlap(I2Src, PairReg)) {
          ++MII;
          continue;
        }
        if (I5Src.isValid() && TRI->regsOverlap(I5Src, PairReg)) {
          ++MII;
          continue;
        }

        // The constant pair must be dead after OR B.
        // For BC: B is overwritten by LD B,A (I4) with the XOR result (same
        // value in both original and folded code), so only C matters.
        // For DE: neither D nor E is overwritten, so both must be dead.
        if (IsBC) {
          if (!isRegDeadAfter(std::next(I7), MBB, TRI, Z80::C)) {
            ++MII;
            continue;
          }
        } else {
          if (!isRegDeadAfter(std::next(I7), MBB, TRI, Z80::DE)) {
            ++MII;
            continue;
          }
        }

        int64_t Imm = MI.getOperand(1).getImm();
        int64_t HiByte = (Imm >> 8) & 0xFF;
        int64_t LoByte = Imm & 0xFF;
        LLVM_DEBUG(dbgs() << "  Folding CMP_Z16 constant: " << MI);

        // Handle XOR rhi: replace with XOR #hi, or remove if hi == 0.
        if (HiByte != 0) {
          BuildMI(MBB, *I3, I3->getDebugLoc(), TII->get(Z80::XOR_n))
              .addImm(HiByte);
        } else {
          // XOR #0 is identity. Also fold LD A,X; LD B,A → LD B,X.
          // I2 is LD A,X, I4 is LD B,A. With XOR removed, this is LD B,X.
          bool Folded = false;
          if (I2Src.isValid()) {
            // Skip LD B,B (self-move NOP when I2Src == B).
            if (I2Src != Z80::B)
              Z80::buildLD8(MBB, *I2, I2->getDebugLoc(), *TII, Z80::B, I2Src);
            Folded = true;
          } else if (isLoadHL(*I2, Z80::A)) {
            Z80::buildLoadHL(MBB, *I2, I2->getDebugLoc(), *TII, Z80::B);
            Folded = true;
          }
          if (Folded) {
            I2->eraseFromParent();
            I4->eraseFromParent();
          }
        }
        I3->eraseFromParent();

        // Handle XOR rlo: replace with XOR #lo, or remove if lo == 0.
        if (LoByte != 0) {
          BuildMI(MBB, *I6, I6->getDebugLoc(), TII->get(Z80::XOR_n))
              .addImm(LoByte);
        }
        I6->eraseFromParent();

        // Remove LD rr,#imm
        MII = MBB.erase(MII);
        Changed = BlockChanged = true;
      }
    }

    // --- Peephole: LD A,(HL); INC/DEC HL → LD A,(HL+)/(HL-) (SM83 only) ---
    // SM83 has post-increment/decrement LD instructions that combine a load
    // or store with an HL adjustment in a single byte.
    // Patterns:
    //   LD A,(HL); INC HL → LD A,(HL+)   (2B → 1B)
    //   LD (HL),A; INC HL → LD (HL+),A   (2B → 1B)
    //   LD A,(HL); DEC HL → LD A,(HL-)   (2B → 1B)
    //   LD (HL),A; DEC HL → LD (HL-),A   (2B → 1B)
    //
    // Extended: when r != A and A is dead after the sequence:
    //   LD r,(HL); INC HL → LD A,(HL+); LD r,A   (2B → 2B, saves 4T)
    //   LD (HL),r; INC HL → LD A,r; LD (HL+),A   (2B → 2B, saves 4T)
    //   (same for DEC HL variants with HL-/HL-)
    if (STI.hasSM83()) {
      for (MachineBasicBlock::iterator MII = MBB.begin(), MIE = MBB.end();
           MII != MIE;) {
        MachineInstr &MI = *MII;
        auto NextIt = std::next(MII);
        if (NextIt == MIE) {
          ++MII;
          continue;
        }

        // Direct r=A patterns: 2B → 1B (size + speed win)
        unsigned NewOpc = 0;
        if (isIncDec16(*NextIt, Z80::INC_rr, Z80::HL)) {
          if (isLoadHL(MI, Z80::A))
            NewOpc = Z80::LD_A_HLI;
          else if (isStoreHL(MI, Z80::A))
            NewOpc = Z80::LD_HLI_A;
        } else if (isIncDec16(*NextIt, Z80::DEC_rr, Z80::HL)) {
          if (isLoadHL(MI, Z80::A))
            NewOpc = Z80::LD_A_HLD;
          else if (isStoreHL(MI, Z80::A))
            NewOpc = Z80::LD_HLD_A;
        }

        // A pair the register allocator filled in one half only reaches here
        // as a byte access reading nothing, and the replacement re-expresses
        // that read without giving it a value.
        SmallVector<MCRegister, 4> Empty;
        Z80::collectUndefReads(MI, TRI, Empty);
        bool AtStart = MII == MBB.begin();
        auto Prev = AtStart ? MBB.end() : std::prev(MII);
        auto newRange = [&] { return AtStart ? MBB.begin() : std::next(Prev); };

        if (NewOpc) {
          LLVM_DEBUG(dbgs() << "  LD+INC/DEC HL → LD (HL+/-): " << MI);
          BuildMI(MBB, MI, MI.getDebugLoc(), TII->get(NewOpc));
          Z80::markEmptyReads(newRange(), MII, TRI, Empty);
          NextIt->eraseFromParent();
          MII = MBB.erase(MII);
          Changed = BlockChanged = true;
          continue;
        }

        // Extended r!=A patterns: 2B → 2B (speed win only, saves 4T)
        // LD r,(HL); INC/DEC HL → LD A,(HL+/-); LD r,A  (requires A dead)
        // LD (HL),r; INC/DEC HL → LD A,r; LD (HL+/-),A  (requires A dead)
        if (isIncDec16(*NextIt, Z80::INC_rr, Z80::HL) ||
            isIncDec16(*NextIt, Z80::DEC_rr, Z80::HL)) {
          bool IsInc = isIncDec16(*NextIt, Z80::INC_rr, Z80::HL);

          Register LoadDst = getLoadHLindDstReg(MI);
          Register StoreSrc = getStoreHLindSrcReg(MI);
          // Exclude A: LD A,(HL) → LD A,(HL+) is handled directly,
          // and LD (HL),A → LD A,A; LD (HL+),A produces a useless LD A,A.
          if (LoadDst == Z80::A)
            LoadDst = Register();
          if (StoreSrc == Z80::A)
            StoreSrc = Register();

          // Skip if this load is part of a 16-bit HL load pattern that the
          // later peephole will fold more profitably (5B → 3B vs our 2B → 2B).
          // Pattern: LD C/E,(HL); INC HL; LD B/D,(HL); LD L,C/E; LD H,B/D
          if (LoadDst.isValid() && IsInc) {
            auto I3 = std::next(NextIt);
            if (I3 != MIE) {
              Register HiReg = (LoadDst == Z80::C)   ? Z80::B
                               : (LoadDst == Z80::E) ? Z80::D
                                                     : Register();
              if (HiReg.isValid() && isLoadHL(*I3, HiReg)) {
                ++MII;
                continue; // Let 16-bit HL load peephole handle it
              }
            }
          }

          if (LoadDst.isValid() || StoreSrc.isValid()) {
            auto AfterSeq = std::next(NextIt);
            if (isRegDeadAfter(AfterSeq, MBB, TRI, Z80::A)) {
              DebugLoc DL = MI.getDebugLoc();
              unsigned HLOpc = IsInc ? Z80::LD_A_HLI : Z80::LD_A_HLD;
              unsigned HLSOpc = IsInc ? Z80::LD_HLI_A : Z80::LD_HLD_A;

              if (LoadDst.isValid()) {
                // LD r,(HL); INC/DEC HL → LD A,(HL+/-); LD r,A
                LLVM_DEBUG(dbgs() << "  LD r,(HL)+INC/DEC → HL+/-: " << MI);
                BuildMI(MBB, MI, DL, TII->get(HLOpc));
                Z80::buildLD8(MBB, MI, DL, *TII, LoadDst, Z80::A);
              } else {
                // LD (HL),r; INC/DEC HL → LD A,r; LD (HL+/-),A
                LLVM_DEBUG(dbgs() << "  LD (HL),r+INC/DEC → HL+/-: " << MI);
                Z80::buildLD8(MBB, MI, DL, *TII, Z80::A, StoreSrc);
                BuildMI(MBB, MI, DL, TII->get(HLSOpc));
              }

              Z80::markEmptyReads(newRange(), MII, TRI, Empty);
              NextIt->eraseFromParent();
              MII = MBB.erase(MII);
              Changed = BlockChanged = true;
              continue;
            }
          }
        }

        ++MII;
      }

      // --- Peephole: 16-bit HL load via HL+ (SM83 only) ---
      // When loading a 16-bit value from (HL) into HL itself via BC or DE:
      //   LD lo,(HL); INC HL; LD hi,(HL); LD L,lo; LD H,hi  (5B)
      // → LD A,(HL+); LD H,(HL); LD L,A                     (3B, saves 2B)
      // LD A,(HL+) loads lo byte and increments HL in one instruction.
      // LD H,(HL) reads the hi byte (HL still points to hi) before writing H.
      // LD L,A completes the 16-bit value in HL.
      // Conditions: A dead after (clobbered), register pair dead after (not
      // loaded).
      for (MachineBasicBlock::iterator MII = MBB.begin(), MIE = MBB.end();
           MII != MIE;) {
        MachineInstr &MI = *MII;

        // I1: LD C,(HL) or LD E,(HL)
        bool IsBC = isLoadHL(MI, Z80::C);
        bool IsDE = isLoadHL(MI, Z80::E);
        if (!IsBC && !IsDE) {
          ++MII;
          continue;
        }

        auto I2 = std::next(MII);
        if (I2 == MIE || !isIncDec16(*I2, Z80::INC_rr, Z80::HL)) {
          ++MII;
          continue;
        }
        auto I3 = std::next(I2);
        if (I3 == MIE) {
          ++MII;
          continue;
        }
        if (!isLoadHL(*I3, IsBC ? Z80::B : Z80::D)) {
          ++MII;
          continue;
        }

        auto I4 = std::next(I3);
        if (I4 == MIE) {
          ++MII;
          continue;
        }
        if (!isLD8(*I4, Z80::L, IsBC ? Z80::C : Z80::E)) {
          ++MII;
          continue;
        }

        auto I5 = std::next(I4);
        if (I5 == MIE) {
          ++MII;
          continue;
        }
        if (!isLD8(*I5, Z80::H, IsBC ? Z80::B : Z80::D)) {
          ++MII;
          continue;
        }

        auto After = std::next(I5);
        if (!isRegDeadAfter(After, MBB, TRI, Z80::A)) {
          ++MII;
          continue;
        }
        MCPhysReg PairReg = IsBC ? Z80::BC : Z80::DE;
        if (!isRegDeadAfter(After, MBB, TRI, PairReg)) {
          ++MII;
          continue;
        }

        LLVM_DEBUG(dbgs() << "  16-bit HL load via HL+: " << MI);
        DebugLoc DL = MI.getDebugLoc();
        BuildMI(MBB, MI, DL, TII->get(Z80::LD_A_HLI));
        Z80::buildLoadHL(MBB, MI, DL, *TII, Z80::H);
        Z80::buildLD8(MBB, MI, DL, *TII, Z80::L, Z80::A);

        I5->eraseFromParent();
        I4->eraseFromParent();
        I3->eraseFromParent();
        I2->eraseFromParent();
        MII = MBB.erase(MII);
        Changed = BlockChanged = true;
      }
    }

    // --- SM83 SP-relative store-to-load forwarding ---
    // On SM83, stack access uses LDHL SP,#N; LD (HL),r / LD (HL),#imm.
    // Track what values (register or immediate) are at each stack offset,
    // then forward to subsequent loads to eliminate redundant LDHL sequences.
    if (MF.getSubtarget<Z80Subtarget>().hasSM83()) {
      // Each slot can hold either a register value or an immediate.
      struct SlotVal {
        bool IsImm = false;
        MCPhysReg Reg = 0;
        uint8_t Imm = 0;
      };
      DenseMap<int, SlotVal, IXOffsetInfo> SPSlots;
      int SPDelta = 0;

      auto invalidateSlotReg = [&](const TargetRegisterInfo *TRI,
                                   MCPhysReg Reg) {
        SmallVector<int, 4> ToErase;
        for (auto &KV : SPSlots) {
          if (!KV.second.IsImm && TRI->regsOverlap(KV.second.Reg, Reg))
            ToErase.push_back(KV.first);
        }
        for (int K : ToErase)
          SPSlots.erase(K);
      };

      // A store of a register that holds nothing leaves nothing in the slot,
      // so the slot must not be described as holding what that register does.
      auto setSlot = [&](int Off, const SlotVal &S, bool Empty) {
        if (Empty)
          SPSlots.erase(Off);
        else
          SPSlots[Off] = S;
      };

      for (MachineBasicBlock::iterator MII = MBB.begin(), MIE = MBB.end();
           MII != MIE;) {
        MachineInstr &MI = *MII;
        unsigned Opc = MI.getOpcode();

        // Track SP changes.
        if (Opc == Z80::PUSH_AF || Opc == Z80::PUSH_BC || Opc == Z80::PUSH_DE ||
            Opc == Z80::PUSH_HL) {
          SPDelta -= 2;
          // PUSH writes to SPDelta+0 and SPDelta+1, invalidate those slots.
          SPSlots.erase(SPDelta);
          SPSlots.erase(SPDelta + 1);
          ++MII;
          continue;
        }
        if (Opc == Z80::POP_AF || Opc == Z80::POP_BC || Opc == Z80::POP_DE ||
            Opc == Z80::POP_HL) {
          // Invalidate slots at the popped location (no longer on stack).
          SPSlots.erase(SPDelta);
          SPSlots.erase(SPDelta + 1);
          SPDelta += 2;
          for (const MachineOperand &MO : MI.operands()) {
            if (MO.isReg() && MO.isDef() && MO.getReg().isPhysical())
              invalidateSlotReg(TRI, MO.getReg());
          }
          ++MII;
          continue;
        }
        if (Opc == Z80::ADD_SP_e) {
          int8_t Adj = (int8_t)(MI.getOperand(0).getImm() & 0xFF);
          SPDelta += Adj;
          ++MII;
          continue;
        }

        // Match LDHL SP,#N followed by store or load pattern.
        if (Opc == Z80::LDHL_SP_e) {
          int8_t Imm = (int8_t)(MI.getOperand(0).getImm() & 0xFF);
          int AbsOff = SPDelta + Imm;

          auto It1 = std::next(MII);
          if (It1 == MIE) {
            ++MII;
            continue;
          }

          // The accesses this address is about to be used for have to be
          // ordinary frame traffic. The longest sequence matched below is
          // three instructions, so that is as far as this has to look.
          bool Plain = true;
          unsigned Look = 3;
          for (auto L = It1; L != MIE && Look; ++L, --Look)
            if (L->mayLoadOrStore() && !isPlainSlotAccess(*L))
              Plain = false;
          if (!Plain) {
            SPSlots.erase(AbsOff);
            SPSlots.erase(AbsOff + 1);
            ++MII;
            continue;
          }

          // Helper: check if a SlotVal matches a new store value.
          auto slotMatches = [](const SlotVal &Slot, bool NewIsImm,
                                MCPhysReg NewReg, uint8_t NewImm) -> bool {
            if (Slot.IsImm != NewIsImm)
              return false;
            if (Slot.IsImm)
              return Slot.Imm == NewImm;
            return Slot.Reg == NewReg;
          };

          // Helper: try to eliminate a redundant 16-bit store sequence.
          // Returns true if eliminated (LDHL + 3 instructions erased).
          auto tryElimRedundantStore =
              [&](int AbsOff, bool LoIsImm, MCPhysReg LoReg, uint8_t LoImm,
                  bool HiIsImm, MCPhysReg HiReg, uint8_t HiImm,
                  MachineBasicBlock::iterator LDHL,
                  MachineBasicBlock::iterator S1,
                  MachineBasicBlock::iterator Mid,
                  MachineBasicBlock::iterator S2) -> bool {
            auto AvLo = SPSlots.find(AbsOff);
            auto AvHi = SPSlots.find(AbsOff + 1);
            if (AvLo == SPSlots.end() || AvHi == SPSlots.end())
              return false;
            if (!slotMatches(AvLo->second, LoIsImm, LoReg, LoImm) ||
                !slotMatches(AvHi->second, HiIsImm, HiReg, HiImm))
              return false;
            // Values match. Safe to remove if HL and FLAGS are dead after.
            auto AfterStore = std::next(S2);
            if (!isRegDeadAfter(AfterStore, MBB, TRI, Z80::HL) ||
                !isRegDeadAfter(AfterStore, MBB, TRI, Z80::FLAGS))
              return false;
            LLVM_DEBUG(dbgs() << "  SM83 eliminating redundant store SP+"
                              << AbsOff << "\n");
            S2->eraseFromParent();
            Mid->eraseFromParent();
            S1->eraseFromParent();
            MII = MBB.erase(LDHL);
            Changed = BlockChanged = true;
            return true;
          };

          // --- 16-bit immediate store: LDHL; LD (HL),#lo; INC HL; LD (HL),#hi
          if (It1->getOpcode() == Z80::LD_HLind_n) {
            auto It2 = std::next(It1);
            if (It2 != MIE && isIncDec16(*It2, Z80::INC_rr, Z80::HL)) {
              auto It3 = std::next(It2);
              if (It3 != MIE && It3->getOpcode() == Z80::LD_HLind_n) {
                uint8_t LoVal = (uint8_t)(It1->getOperand(0).getImm() & 0xFF);
                uint8_t HiVal = (uint8_t)(It3->getOperand(0).getImm() & 0xFF);
                // Try redundant store elimination.
                if (tryElimRedundantStore(AbsOff, true, 0, LoVal, true, 0,
                                          HiVal, MII, It1, It2, It3))
                  continue;
                SlotVal SLo, SHi;
                SLo.IsImm = true;
                SLo.Imm = LoVal;
                SHi.IsImm = true;
                SHi.Imm = HiVal;
                SPSlots[AbsOff] = SLo;
                SPSlots[AbsOff + 1] = SHi;
                LLVM_DEBUG(dbgs() << "  SM83 imm store SP+" << AbsOff << " <- #"
                                  << (int)SLo.Imm << ", SP+" << (AbsOff + 1)
                                  << " <- #" << (int)SHi.Imm << "\n");
                MII = std::next(It3);
                continue;
              }
            }
            // 8-bit immediate store
            SlotVal S;
            S.IsImm = true;
            S.Imm = (uint8_t)(It1->getOperand(0).getImm() & 0xFF);
            SPSlots[AbsOff] = S;
            MII = std::next(It1);
            continue;
          }

          // --- 16-bit register store: LDHL; LD (HL),rlo; INC HL; LD (HL),rhi
          Register StoreSrc1 = getStoreHLindSrcReg(*It1);
          if (StoreSrc1.isValid()) {
            auto It2 = std::next(It1);
            if (It2 != MIE && isIncDec16(*It2, Z80::INC_rr, Z80::HL)) {
              auto It3 = std::next(It2);
              if (It3 != MIE) {
                Register StoreSrc2 = getStoreHLindSrcReg(*It3);
                if (StoreSrc2.isValid()) {
                  // Try redundant store elimination.
                  if (tryElimRedundantStore(AbsOff, false, StoreSrc1, 0, false,
                                            StoreSrc2, 0, MII, It1, It2, It3))
                    continue;
                  SlotVal SLo, SHi;
                  SLo.Reg = StoreSrc1;
                  SHi.Reg = StoreSrc2;
                  setSlot(AbsOff, SLo, Z80::readsUndef(*It1, StoreSrc1));
                  setSlot(AbsOff + 1, SHi, Z80::readsUndef(*It3, StoreSrc2));
                  LLVM_DEBUG(dbgs() << "  SM83 reg store SP+" << AbsOff
                                    << " <- " << printReg(StoreSrc1, TRI)
                                    << ", SP+" << (AbsOff + 1) << " <- "
                                    << printReg(StoreSrc2, TRI) << "\n");
                  MII = std::next(It3);
                  continue;
                }
              }
            }
            // 8-bit register store
            SlotVal S;
            S.Reg = StoreSrc1;
            setSlot(AbsOff, S, Z80::readsUndef(*It1, StoreSrc1));
            MII = std::next(It1);
            continue;
          }

          // --- HL+ register store: LDHL; LD A,r; LD (HL+),A; LD (HL),r2
          {
            Register SrcLo = getLD8Src(*It1, Z80::A);
            // Only B/C/D/E — H/L can't be source (LDHL clobbered HL).
            if (SrcLo.isValid() && SrcLo != Z80::H && SrcLo != Z80::L) {
              auto It2 = std::next(It1);
              if (It2 != MIE && It2->getOpcode() == Z80::LD_HLI_A) {
                auto It3 = std::next(It2);
                if (It3 != MIE) {
                  Register StoreSrc2 = getStoreHLindSrcReg(*It3);
                  if (StoreSrc2.isValid()) {
                    // Try redundant store elimination.
                    if (tryElimRedundantStore(AbsOff, false, SrcLo, 0, false,
                                              StoreSrc2, 0, MII, It1, It2, It3))
                      continue;
                    SlotVal SLo, SHi;
                    SLo.Reg = SrcLo;
                    SHi.Reg = StoreSrc2;
                    setSlot(AbsOff, SLo, Z80::readsUndef(*It1, SrcLo));
                    setSlot(AbsOff + 1, SHi, Z80::readsUndef(*It3, StoreSrc2));
                    LLVM_DEBUG(dbgs() << "  SM83 HL+ store SP+" << AbsOff
                                      << " <- " << printReg(SrcLo, TRI)
                                      << ", SP+" << (AbsOff + 1) << " <- "
                                      << printReg(StoreSrc2, TRI) << "\n");
                    MII = std::next(It3);
                    continue;
                  }
                }
              }
            }
          }

          // --- 16-bit load: LDHL; LD lo,(HL); INC HL; LD hi,(HL)
          Register LoadDst1 = getLoadHLindDstReg(*It1);
          if (LoadDst1.isValid()) {
            auto It2 = std::next(It1);
            if (It2 != MIE && isIncDec16(*It2, Z80::INC_rr, Z80::HL)) {
              auto It3 = std::next(It2);
              if (It3 != MIE) {
                Register LoadDst2 = getLoadHLindDstReg(*It3);
                if (LoadDst2.isValid()) {
                  auto AvLo = SPSlots.find(AbsOff);
                  auto AvHi = SPSlots.find(AbsOff + 1);
                  if (AvLo != SPSlots.end() && AvHi != SPSlots.end()) {
                    SlotVal &SLo = AvLo->second;
                    SlotVal &SHi = AvHi->second;
                    // Forwarding removes LDHL which sets HL and FLAGS.
                    // Verify both are dead after the load sequence.
                    auto AfterLoad = std::next(It3);
                    if (!isRegDeadAfter(AfterLoad, MBB, TRI, Z80::HL) ||
                        !isRegDeadAfter(AfterLoad, MBB, TRI, Z80::FLAGS)) {
                      // Can't forward — fall through to tracking update.
                      invalidateSlotReg(TRI, LoadDst1);
                      invalidateSlotReg(TRI, LoadDst2);
                      SPSlots[AbsOff] = {false, MCPhysReg(LoadDst1), 0};
                      SPSlots[AbsOff + 1] = {false, MCPhysReg(LoadDst2), 0};
                      MII = std::next(It3);
                      continue;
                    }
                    // Build replacement instructions.
                    DebugLoc DL = MI.getDebugLoc();
                    bool CanForward = true;
                    // For register sources: can't use H/L (LDHL clobbers).
                    if (!SLo.IsImm && (SLo.Reg == Z80::H || SLo.Reg == Z80::L))
                      CanForward = false;
                    if (!SHi.IsImm && (SHi.Reg == Z80::H || SHi.Reg == Z80::L))
                      CanForward = false;
                    // Check reg-reg copy feasibility and ordering.
                    if (CanForward && !SLo.IsImm && !SHi.IsImm) {
                      // Both register: check for circular dependency.
                      bool LoIsNop = (LoadDst1 == SLo.Reg);
                      bool HiIsNop = (LoadDst2 == SHi.Reg);
                      bool CopyLo = !LoIsNop && Z80::canLD8(LoadDst1, SLo.Reg);
                      bool CopyHi = !HiIsNop && Z80::canLD8(LoadDst2, SHi.Reg);
                      if (!LoIsNop && !CopyLo)
                        CanForward = false;
                      if (!HiIsNop && !CopyHi)
                        CanForward = false;
                      if (CanForward) {
                        bool HiFirst = TRI->regsOverlap(LoadDst1, SHi.Reg);
                        if (HiFirst && TRI->regsOverlap(LoadDst2, SLo.Reg))
                          CanForward = false; // Circular.
                        if (CanForward) {
                          LLVM_DEBUG(dbgs() << "  SM83 fwd 16-bit reg SP+"
                                            << AbsOff << "\n");
                          auto emitCopyHi = [&] {
                            if (CopyHi)
                              Z80::buildLD8(MBB, MI, DL, *TII, LoadDst2,
                                            SHi.Reg);
                          };
                          auto emitCopyLo = [&] {
                            if (CopyLo)
                              Z80::buildLD8(MBB, MI, DL, *TII, LoadDst1,
                                            SLo.Reg);
                          };
                          if (HiFirst) {
                            emitCopyHi();
                            emitCopyLo();
                          } else {
                            emitCopyLo();
                            emitCopyHi();
                          }
                          It3->eraseFromParent();
                          It2->eraseFromParent();
                          It1->eraseFromParent();
                          MII = MBB.erase(MII);
                          Changed = BlockChanged = true;
                          invalidateSlotReg(TRI, LoadDst1);
                          invalidateSlotReg(TRI, LoadDst2);
                          SPSlots[AbsOff] = {false, MCPhysReg(LoadDst1), 0};
                          SPSlots[AbsOff + 1] = {false, MCPhysReg(LoadDst2), 0};
                          continue;
                        }
                      }
                    }
                    // At least one immediate: generate LD r,#imm for imm
                    // slots and LD r,src for register slots.
                    if (CanForward) {
                      // Pre-validate all opcodes before emitting anything,
                      // to avoid partially-emitted instructions on failure.
                      auto slotIsLoadable = [&](Register Dst, SlotVal &S) {
                        if (S.IsImm)
                          return Z80::isEncodableGR8(Dst);
                        return Dst == S.Reg || Z80::canLD8(Dst, S.Reg);
                      };
                      if (!slotIsLoadable(LoadDst1, SLo) ||
                          !slotIsLoadable(LoadDst2, SHi))
                        CanForward = false;
                    }
                    if (CanForward) {
                      bool HiFirst = false;
                      if (!SHi.IsImm && TRI->regsOverlap(LoadDst1, SHi.Reg))
                        HiFirst = true;

                      LLVM_DEBUG(dbgs() << "  SM83 fwd 16-bit imm/reg SP+"
                                        << AbsOff << "\n");
                      auto emitLoad = [&](Register Dst, SlotVal &S) {
                        if (S.IsImm) {
                          Z80::buildLD8n(MBB, MI, DL, *TII, Dst).addImm(S.Imm);
                        } else if (Dst != S.Reg) {
                          Z80::buildLD8(MBB, MI, DL, *TII, Dst, S.Reg);
                        }
                      };
                      if (HiFirst) {
                        emitLoad(LoadDst2, SHi);
                        emitLoad(LoadDst1, SLo);
                      } else {
                        emitLoad(LoadDst1, SLo);
                        emitLoad(LoadDst2, SHi);
                      }
                      It3->eraseFromParent();
                      It2->eraseFromParent();
                      It1->eraseFromParent();
                      MII = MBB.erase(MII);
                      Changed = BlockChanged = true;
                      invalidateSlotReg(TRI, LoadDst1);
                      invalidateSlotReg(TRI, LoadDst2);
                      SPSlots[AbsOff] = {false, MCPhysReg(LoadDst1), 0};
                      SPSlots[AbsOff + 1] = {false, MCPhysReg(LoadDst2), 0};
                      continue;
                    }
                  }
                  // Couldn't forward — update tracking.
                  invalidateSlotReg(TRI, LoadDst1);
                  invalidateSlotReg(TRI, LoadDst2);
                  SPSlots[AbsOff] = {false, MCPhysReg(LoadDst1), 0};
                  SPSlots[AbsOff + 1] = {false, MCPhysReg(LoadDst2), 0};
                  MII = std::next(It3);
                  continue;
                }
              }
            }
            // 8-bit load: LDHL; LD r,(HL)
            // Forwarding removes LDHL (sets HL/FLAGS), so both must be dead.
            auto After8 = std::next(It1);
            if (!isRegDeadAfter(After8, MBB, TRI, Z80::HL) ||
                !isRegDeadAfter(After8, MBB, TRI, Z80::FLAGS)) {
              invalidateSlotReg(TRI, LoadDst1);
              SPSlots[AbsOff] = {false, MCPhysReg(LoadDst1), 0};
              MII = std::next(It1);
              continue;
            }
            auto AvIt = SPSlots.find(AbsOff);
            if (AvIt != SPSlots.end()) {
              SlotVal &S = AvIt->second;
              DebugLoc DL = MI.getDebugLoc();
              bool Done = false;
              if (S.IsImm) {
                if (Z80::isEncodableGR8(LoadDst1)) {
                  LLVM_DEBUG(dbgs() << "  SM83 fwd 8-bit imm SP+" << AbsOff
                                    << " #" << (int)S.Imm << "\n");
                  Z80::buildLD8n(MBB, MI, DL, *TII, LoadDst1).addImm(S.Imm);
                  It1->eraseFromParent();
                  MII = MBB.erase(MII);
                  Changed = BlockChanged = true;
                  Done = true;
                }
              } else if (S.Reg != Z80::H && S.Reg != Z80::L) {
                bool NeedCopy =
                    LoadDst1 != S.Reg && Z80::canLD8(LoadDst1, S.Reg);
                if (LoadDst1 == S.Reg || NeedCopy) {
                  LLVM_DEBUG(dbgs()
                             << "  SM83 fwd 8-bit reg SP+" << AbsOff << "\n");
                  if (NeedCopy)
                    Z80::buildLD8(MBB, MI, DL, *TII, LoadDst1, S.Reg);
                  It1->eraseFromParent();
                  MII = MBB.erase(MII);
                  Changed = BlockChanged = true;
                  Done = true;
                }
              }
              if (Done) {
                invalidateSlotReg(TRI, LoadDst1);
                SPSlots[AbsOff] = {false, MCPhysReg(LoadDst1), 0};
                continue;
              }
            }
            invalidateSlotReg(TRI, LoadDst1);
            SPSlots[AbsOff] = {false, MCPhysReg(LoadDst1), 0};
            MII = std::next(It1);
            continue;
          }

          // LDHL not followed by a recognizable pattern — HL is clobbered.
          invalidateSlotReg(TRI, Z80::HL);
          ++MII;
          continue;
        }

        // Calls and unmodeled side effects clear everything.
        if (MI.isCall() || MI.hasUnmodeledSideEffects()) {
          SPSlots.clear();
          ++MII;
          continue;
        }

        // Any other instruction: invalidate entries for defined regs.
        for (const MachineOperand &MO : MI.operands()) {
          if (MO.isReg() && MO.isDef() && MO.getReg().isPhysical())
            invalidateSlotReg(TRI, MO.getReg());
        }
        for (MCPhysReg Def : TII->get(Opc).implicit_defs())
          invalidateSlotReg(TRI, Def);
        ++MII;
      }
    }

    // --- Store-to-load forwarding and register copy elimination ---
    // Map from IX offset to the physical register holding that value.
    DenseMap<int, MCPhysReg, IXOffsetInfo> AvailValues;

    for (MachineBasicBlock::iterator MII = MBB.begin(), MIE = MBB.end();
         MII != MIE;) {
      MachineInstr &MI = *MII++;
      unsigned Opc = MI.getOpcode();

      // Case 1: IX-indexed store — LD (IX+d), R
      Register StoreSrc = getStoreIXdSrcReg(MI);
      if (StoreSrc.isValid()) {
        int Offset = Z80::idxSlotOperand(MI).getImm();
        if (isPlainSlotAccess(MI) && !Z80::readsUndef(MI, StoreSrc))
          AvailValues[Offset] = StoreSrc;
        else
          AvailValues.erase(Offset);
        continue;
      }

      // Case 2: IX-indexed load — LD R', (IX+d)
      Register LoadDst = getLoadIXdDstReg(MI);
      if (LoadDst.isValid()) {
        int Offset = Z80::idxSlotOperand(MI).getImm();

        if (isPlainSlotAccess(MI)) {
          auto It = AvailValues.find(Offset);
          if (It != AvailValues.end()) {
            MCPhysReg SrcReg = It->second;
            if (LoadDst == SrcReg) {
              // LD R, (IX+d) where R already holds the value — no-op.
              // Don't invalidate anything: R's value doesn't change.
              LLVM_DEBUG(dbgs() << "  Eliminating redundant reload: " << MI);
              MI.eraseFromParent();
              Changed = BlockChanged = true;
              continue;
            }
            // Replace LD R', (IX+d) with LD R', R_src.
            if (Z80::canLD8(LoadDst, SrcReg)) {
              LLVM_DEBUG(dbgs() << "  Forwarding: " << MI << "  -> LD "
                                << printReg(LoadDst, TRI) << ", "
                                << printReg(SrcReg, TRI) << "\n");
              // R' gets a new value — invalidate other entries pointing to R'.
              invalidateReg(AvailValues, TRI, LoadDst);
              Z80::buildLD8(MBB, MI, MI.getDebugLoc(), *TII, LoadDst, SrcReg);
              MI.eraseFromParent();
              Changed = BlockChanged = true;
              AvailValues[Offset] = LoadDst;
              continue;
            }
          }
        }
        // Couldn't forward — R' gets a new value from memory.
        // Invalidate entries pointing to R' (they're stale).
        invalidateReg(AvailValues, TRI, LoadDst);
        // R' now holds the value at offset d.
        if (isPlainSlotAccess(MI))
          AvailValues[Offset] = LoadDst;
        continue;
      }

      // Case 3: LD (IX+d), n — immediate store to IX slot
      if (Opc == Z80::LD_IXd_n) {
        int Offset = Z80::idxSlotOperand(MI).getImm();
        AvailValues.erase(Offset);
        continue;
      }

      // Case 4: Calls and unmodeled side effects — clear everything.
      if (MI.isCall() || MI.hasUnmodeledSideEffects()) {
        AvailValues.clear();
        continue;
      }

      // Case 5: Any other instruction — invalidate entries for defined regs.
      for (const MachineOperand &MO : MI.operands()) {
        if (MO.isReg() && MO.isDef() && MO.getReg().isPhysical())
          invalidateReg(AvailValues, TRI, MO.getReg());
      }
      // Also check implicit defs from the instruction descriptor.
      for (MCPhysReg Def : TII->get(Opc).implicit_defs())
        invalidateReg(AvailValues, TRI, Def);
    }

    if (BlockChanged)
      recomputeLivenessFlags(MBB);
  }

  // Run last: earlier peepholes pattern-match LDHL-based slot accesses
  // (redundant store elimination keys slot identity on them), so the
  // address-reuse rewrite must not obscure those first.
  for (MachineBasicBlock &MBB : MF) {
    if (STI.hasSM83()) {
      Changed |= reuseLDHLAddress(MBB, TII, TRI);
      // After address reuse: it creates the INC HL neighbors these fuse with.
      Changed |= materializeConstantStores(MBB, TII, TRI);
      Changed |= fusePostIncAccess(MBB, TII, TRI);
      // After fusion: the reload it rewrites only takes its LD A,(HL+) form
      // once these two have run.
      Changed |= reloadDirectlyIntoPair(MBB, TII, TRI);
    }
    // The store itself costs the same either way, so the LD A,n that pays
    // for the shorter stores is pure added time: a size-for-speed trade,
    // which is the level that takes those.
    if (!STI.hasSM83() && MF.getFunction().hasMinSize())
      Changed |= materializeIXConstantStores(MBB, TII, TRI);
    Changed |= directIncDec(MBB, TII, TRI);
    Changed |= elidePopPushAcrossStretch(MBB, TII, TRI);
    Changed |= elidePushPopAcrossStretch(MBB, TII, TRI);
  }

  return Changed;
}

} // namespace

char Z80LateOptimization::ID = 0;

INITIALIZE_PASS(Z80LateOptimization, DEBUG_TYPE, "Z80 Late Optimizations",
                false, false)

MachineFunctionPass *llvm::createZ80LateOptimizationPass() {
  return new Z80LateOptimization;
}
