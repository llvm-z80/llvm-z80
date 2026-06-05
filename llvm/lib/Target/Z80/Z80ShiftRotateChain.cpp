//===-- Z80ShiftRotateChain.cpp - Z80 Shift/Rotate Chaining ---------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file Z80 shift/rotate chaining pass.
///
/// Z80 has no barrel shifter — shifts and rotates are implemented by repeating
/// a 1-bit operation N times (e.g. SLA A executed 5 times for x << 5).  This
/// makes the cost O(N) in the shift amount, unlike most targets where it is
/// O(1).
///
/// When the same base value is shifted by different constant amounts with the
/// same opcode, the later shift can reuse the result of the earlier one:
///
///   Before:                          After:
///   %a = G_SHL %x, 3   (3 iters)    %a = G_SHL %x, 3   (3 iters)
///   %b = G_SHL %x, 5   (5 iters)    %b = G_SHL %a, 2   (2 iters)
///                       total: 8                         total: 5
///
/// Algorithm:
///   1. Collect: for each vreg, gather all constant-amount shift/rotate users
///      of the same opcode (G_SHL, G_ASHR, G_LSHR, G_ROTL, G_ROTR).
///   2. Sort: group by opcode, then by amount ascending.
///   3. Chain: rewrite each shift to use the previous one's result, with the
///      amount reduced by the difference.
///   4. Dominance: if the previous shift doesn't dominate the current one,
///      move it (and its constant operand) to the nearest common dominator.
///
/// Only constant shift amounts are handled.  Variable amounts would require
/// runtime subtraction and are rarely chainable in practice.
///
/// Note: the standard combiner's shift_immed_chain does the OPPOSITE — it
/// merges chained shifts into one (SHL(SHL(x,3),2) → SHL(x,5)).  Both
/// transforms are correct; this pass runs after the combiner and creates
/// chains that are beneficial for Z80's linear-cost shift model.
///
/// Based on MOSShiftRotateChain from LLVM-MOS.
//
//===----------------------------------------------------------------------===//

#include "Z80ShiftRotateChain.h"

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80.h"
#include "llvm/ADT/IndexedMap.h"
#include "llvm/CodeGen/GlobalISel/MachineIRBuilder.h"
#include "llvm/CodeGen/GlobalISel/Utils.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"

#define DEBUG_TYPE "z80-shift-rotate-chain"

using namespace llvm;

namespace {

class Z80ShiftRotateChain : public MachineFunctionPass {
public:
  static char ID;

  Z80ShiftRotateChain() : MachineFunctionPass(ID) {
    llvm::initializeZ80ShiftRotateChainPass(*PassRegistry::getPassRegistry());
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    MachineFunctionPass::getAnalysisUsage(AU);
    AU.addRequired<MachineDominatorTreeWrapperPass>();
    AU.addPreserved<MachineDominatorTreeWrapperPass>();
  }

  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties().set(
        MachineFunctionProperties::Property::IsSSA);
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  // Move PrevMI and the values it depends on up the dominance tree until it
  // dominates MI, all the way up to base.
  void ensureDominates(const MachineInstr &Base, MachineInstr &PrevMI,
                       MachineInstr &MI) const;
};

bool Z80ShiftRotateChain::runOnMachineFunction(MachineFunction &MF) {
  struct ChainEntry {
    Register R;
    unsigned Opcode;
    unsigned Amount;
  };
  typedef SmallVector<ChainEntry> Chain;
  IndexedMap<Chain, VirtReg2IndexFunctor> Chains;

  LLVM_DEBUG(dbgs() << "\n\nChaining shifts and rotates in: " << MF.getName()
                    << "\n\n");

  const MachineRegisterInfo &MRI = MF.getRegInfo();

  Chains.resize(MRI.getNumVirtRegs());
  for (unsigned I = 0, E = MRI.getNumVirtRegs(); I != E; ++I) {
    Register R = Register::index2VirtReg(I);
    MachineInstr *MI = MRI.getUniqueVRegDef(R);
    if (!MI)
      continue;

    unsigned Opcode = MI->getOpcode();
    switch (Opcode) {
    case Z80::G_SHL:
    case Z80::G_ASHR:
    case Z80::G_LSHR:
    case Z80::G_ROTL:
    case Z80::G_ROTR:
      break;
    default:
      continue;
    }

    Register LHS = MI->getOperand(1).getReg();
    Register RHS = MI->getOperand(2).getReg();
    auto RHSConst = getIConstantVRegValWithLookThrough(RHS, MRI);
    if (!RHSConst)
      continue;
    unsigned RHSValue = RHSConst->Value.getZExtValue();
    Chains[LHS].push_back({R, Opcode, RHSValue});
  }

  for (unsigned I = 0, E = MRI.getNumVirtRegs(); I != E; ++I) {
    Register R = Register::index2VirtReg(I);

    llvm::sort(Chains[R], [](const ChainEntry &L, const ChainEntry &R) {
      if (L.Opcode < R.Opcode)
        return true;
      if (L.Opcode > R.Opcode)
        return false;
      return L.Amount < R.Amount;
    });

    LLVM_DEBUG({
      if (!Chains[R].empty()) {
        const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
        dbgs() << "Creating chain for " << printReg(R) << ":\n";
        for (const ChainEntry &C : Chains[R]) {
          dbgs() << printReg(C.R) << " := " << TII.getName(C.Opcode) << ' '
                 << C.Amount << '\n';
        }
        dbgs() << '\n';
      }
    });
  }

  bool Changed = false;
  for (unsigned I = 0, E = MRI.getNumVirtRegs(); I != E; ++I) {
    Register R = Register::index2VirtReg(I);
    if (Chains[R].empty())
      continue;

    for (unsigned J = 0, JE = Chains[R].size(); J != JE; ++J) {
      ChainEntry &C = Chains[R][J];
      if (!J || C.Opcode != Chains[R][J - 1].Opcode)
        continue;
      ChainEntry &Prev = Chains[R][J - 1];

      MachineInstr &MI = *MRI.getUniqueVRegDef(C.R);
      MachineInstr &PrevMI = *MRI.getUniqueVRegDef(Prev.R);

      ensureDominates(*MRI.getUniqueVRegDef(R), PrevMI, MI);

      Changed = true;
      MI.getOperand(1).setReg(PrevMI.getOperand(0).getReg());
      MachineIRBuilder B(MI);
      MI.getOperand(2).setReg(
          B.buildConstant(MRI.getType(C.R), C.Amount - Prev.Amount).getReg(0));
    }
  }
  return Changed;
}

void Z80ShiftRotateChain::ensureDominates(const MachineInstr &Base,
                                          MachineInstr &PrevMI,
                                          MachineInstr &MI) const {
  auto &MDT = getAnalysis<MachineDominatorTreeWrapperPass>().getDomTree();
  if (MDT.dominates(&PrevMI, &MI))
    return;

  const auto &MRI = PrevMI.getParent()->getParent()->getRegInfo();

  MachineBasicBlock *MBB;
  MachineBasicBlock::iterator InsertPt;
  if (MDT.dominates(&MI, &PrevMI)) {
    MBB = MI.getParent();
    InsertPt = MI;
  } else {
    MBB = MDT.findNearestCommonDominator(PrevMI.getParent(), MI.getParent());
    InsertPt = MBB->getFirstTerminator();
  }

  MBB->insert(InsertPt, PrevMI.removeFromParent());
  PrevMI.setDebugLoc(MBB->findDebugLoc(InsertPt));
  MachineInstr &AmountMI = *MRI.getUniqueVRegDef(PrevMI.getOperand(2).getReg());
  if (!MDT.dominates(&AmountMI, &PrevMI))
    MBB->insert(PrevMI, AmountMI.removeFromParent());

  MachineInstr &ImmBase = *MRI.getUniqueVRegDef(PrevMI.getOperand(1).getReg());
  if (&ImmBase != &Base)
    ensureDominates(Base, ImmBase, PrevMI);
}

} // namespace

char Z80ShiftRotateChain::ID = 0;

INITIALIZE_PASS(Z80ShiftRotateChain, DEBUG_TYPE, "Z80 Shift/Rotate Chaining",
                false, false)

MachineFunctionPass *llvm::createZ80ShiftRotateChainPass() {
  return new Z80ShiftRotateChain();
}
