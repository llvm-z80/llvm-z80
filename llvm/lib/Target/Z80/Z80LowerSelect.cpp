//===-- Z80LowerSelect.cpp - Z80 Select Lowering --------------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file Lower G_SELECT to a triangle control-flow pattern.
///
/// Z80 has no conditional move instruction, so G_SELECT must be expanded to
/// branches.  We create the following pattern:
///
///   HeadMBB:
///     ...
///     G_BRCOND %cond, SinkMBB       ; if true, skip to SinkMBB
///
///   FalseMBB:                        ; fallthrough
///     G_BR SinkMBB
///
///   SinkMBB:
///     %result = G_PHI [%true, HeadMBB], [%false, FalseMBB]
///     ...
///
/// This runs before RegBankSelect so that PHI nodes are visible to register
/// allocation, enabling better live range decisions.
///
/// After expansion, comparison instructions that ended up in SinkMBB (from
/// code originally after the G_SELECT) are moved adjacent to their G_BRCOND
/// users, enabling compare+branch fusion in instruction selection.
///
/// Based on MOSLowerSelect from LLVM-MOS.  Simplified from a diamond to a
/// triangle pattern and extended with compare sinking for branch fusion.
//
//===----------------------------------------------------------------------===//

#include "Z80LowerSelect.h"

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80.h"
#include "llvm/CodeGen/GlobalISel/GenericMachineInstrs.h"
#include "llvm/CodeGen/GlobalISel/MachineIRBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"

#define DEBUG_TYPE "z80-lower-select"

using namespace llvm;

namespace {

class Z80LowerSelect : public MachineFunctionPass {
public:
  static char ID;

  Z80LowerSelect() : MachineFunctionPass(ID) {
    llvm::initializeZ80LowerSelectPass(*PassRegistry::getPassRegistry());
  }

  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties()
        .set(MachineFunctionProperties::Property::IsSSA)
        .set(MachineFunctionProperties::Property::Legalized);
  }

  MachineFunctionProperties getClearedProperties() const override {
    return MachineFunctionProperties().set(
        MachineFunctionProperties::Property::NoPHIs);
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  MachineFunction::reverse_iterator lowerSelect(GSelect &MI);
  void sinkComparesForBranchFusion(MachineBasicBlock &SinkMBB);

  static bool isCompare(unsigned Opc) {
    return Opc == TargetOpcode::G_ICMP || Opc == Z80::G_Z80_ICMP32 ||
           Opc == Z80::G_Z80_ICMP64;
  }
};

bool Z80LowerSelect::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "\n\nHandling G_SELECTs in: " << MF.getName() << "\n\n");

  bool Changed = false;
  for (auto I = MF.rbegin(), E = MF.rend(); I != E; ++I) {
    for (MachineInstr &MBBI : mbb_reverse(*I)) {
      if (auto *S = dyn_cast<GSelect>(&MBBI)) {
        LLVM_DEBUG(dbgs() << "Lowering: " << *S);
        Changed = true;
        I = lowerSelect(*S);
        break;
      }
    }
  }
  return Changed;
}

MachineFunction::reverse_iterator Z80LowerSelect::lowerSelect(GSelect &MI) {
  Register Dst = MI.getOperand(0).getReg();
  Register Cond = MI.getCondReg();
  Register TrueValue = MI.getTrueReg();
  Register FalseValue = MI.getFalseReg();

  MachineIRBuilder Builder(MI);
  MachineBasicBlock &HeadMBB = Builder.getMBB();
  MachineFunction &MF = Builder.getMF();
  const BasicBlock *LLVM_BB = HeadMBB.getBasicBlock();
  MachineFunction::iterator InsertPt = std::next(HeadMBB.getIterator());

  // Create the triangle pattern:
  //   HeadMBB → (cond true) → SinkMBB
  //           → (fallthrough) → FalseMBB → SinkMBB
  MachineBasicBlock *FalseMBB = MF.CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *SinkMBB = MF.CreateMachineBasicBlock(LLVM_BB);
  MF.insert(InsertPt, FalseMBB);
  MF.insert(InsertPt, SinkMBB);

  // Move everything after the G_SELECT to SinkMBB.
  SinkMBB->splice(SinkMBB->begin(), &HeadMBB, std::next(MI.getIterator()),
                  HeadMBB.end());
  SinkMBB->transferSuccessorsAndUpdatePHIs(&HeadMBB);

  // HeadMBB: branch to SinkMBB if true, fallthrough to FalseMBB.
  HeadMBB.addSuccessor(SinkMBB);
  HeadMBB.addSuccessor(FalseMBB);
  Builder.buildBrCond(Cond, *SinkMBB);
  Builder.buildBr(*FalseMBB);

  // FalseMBB: unconditional branch to SinkMBB.
  FalseMBB->addSuccessor(SinkMBB);
  Builder.setInsertPt(*FalseMBB, FalseMBB->end());
  Builder.buildBr(*SinkMBB);

  // SinkMBB: PHI selects the result.
  Builder.setInsertPt(*SinkMBB, SinkMBB->begin());
  Builder.buildInstr(TargetOpcode::G_PHI)
      .addDef(Dst)
      .addUse(TrueValue)
      .addMBB(&HeadMBB)
      .addUse(FalseValue)
      .addMBB(FalseMBB);

  MI.eraseFromParent();

  sinkComparesForBranchFusion(*SinkMBB);

  return MachineFunction::reverse_iterator(*SinkMBB);
}

/// Move comparison instructions to be adjacent to their G_BRCOND users in
/// SinkMBB.  After G_SELECT expansion, a G_BRCOND that was originally after the
/// G_SELECT ends up in SinkMBB, but its G_ICMP condition may remain in HeadMBB.
/// Moving it enables the instruction selector to fuse CMP + JP_cc.
void Z80LowerSelect::sinkComparesForBranchFusion(MachineBasicBlock &SinkMBB) {
  MachineRegisterInfo &MRI = SinkMBB.getParent()->getRegInfo();

  for (MachineInstr &MI : SinkMBB) {
    if (MI.getOpcode() != TargetOpcode::G_BRCOND)
      continue;

    Register CondReg = MI.getOperand(0).getReg();
    MachineInstr *CondDef = MRI.getVRegDef(CondReg);
    if (!CondDef || !MRI.hasOneNonDBGUse(CondReg))
      continue;

    // Look through G_FREEZE (semantically a no-op for codegen).
    MachineInstr *CmpDef = CondDef;
    if (CondDef->getOpcode() == TargetOpcode::G_FREEZE) {
      Register SrcReg = CondDef->getOperand(1).getReg();
      MachineInstr *SrcDef = MRI.getVRegDef(SrcReg);
      if (!SrcDef || !MRI.hasOneNonDBGUse(SrcReg))
        continue;
      CmpDef = SrcDef;
    }

    if (!isCompare(CmpDef->getOpcode()) || CmpDef->getParent() == &SinkMBB)
      continue;

    // Move the comparison (and G_FREEZE if present) directly before G_BRCOND.
    CmpDef->moveBefore(&MI);
    if (CondDef != CmpDef && CondDef->getParent() != &SinkMBB)
      CondDef->moveBefore(&MI);
  }
}

} // namespace

char Z80LowerSelect::ID = 0;

INITIALIZE_PASS(Z80LowerSelect, DEBUG_TYPE,
                "Lower Z80 Select pseudo-instruction", false, false)

MachineFunctionPass *llvm::createZ80LowerSelectPass() {
  return new Z80LowerSelect();
}
