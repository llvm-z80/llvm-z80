//===-- Z80BranchCleanup.cpp - Z80 Branch Relaxation Cleanup --------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// BranchRelaxation handles out-of-range JR_CC by reversing the condition and
// inserting a JP trampoline in a new MBB:
//
//   bb.0:
//     JR_CC_rev bb.1       ; 2 bytes — reversed condition, near target
//   bb.trampoline:
//     JP bb.far             ; 3 bytes — unconditional jump to far target
//   bb.1:                   ; (total: 5 bytes, 2 branches on cold path)
//
// Since Z80 has JP CC,nn (conditional absolute jump), this pass collapses
// the trampoline by moving the JP_CC into the predecessor block:
//
//   bb.0:
//     JP_CC bb.far          ; 3 bytes — original condition, direct jump
//   bb.1:                   ; (total: 3 bytes, 0 branches on hot path)
//
// The now-empty trampoline block is removed.
// Must run after BranchRelaxation.
//
//===----------------------------------------------------------------------===//

#include "Z80BranchCleanup.h"

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80.h"
#include "Z80Subtarget.h"

#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "z80-branch-cleanup"

using namespace llvm;

namespace {

class Z80BranchCleanup : public MachineFunctionPass {
public:
  static char ID;

  Z80BranchCleanup() : MachineFunctionPass(ID) {
    initializeZ80BranchCleanupPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
};

} // namespace

/// Invert a JR condition code to the corresponding JP condition code.
/// JR_Z  → JP_NZ, JR_NZ → JP_Z, JR_C  → JP_NC, JR_NC → JP_C.
/// Returns 0 if the opcode is not a conditional JR.
static unsigned invertJRtoJP(unsigned JROpc) {
  switch (JROpc) {
  case Z80::JR_Z_e:
    return Z80::JP_NZ_nn;
  case Z80::JR_NZ_e:
    return Z80::JP_Z_nn;
  case Z80::JR_C_e:
    return Z80::JP_NC_nn;
  case Z80::JR_NC_e:
    return Z80::JP_C_nn;
  default:
    return 0;
  }
}

bool Z80BranchCleanup::runOnMachineFunction(MachineFunction &MF) {
  const auto &STI = MF.getSubtarget<Z80Subtarget>();
  const auto *TII = STI.getInstrInfo();
  bool Changed = false;

  // Collect trampoline blocks to remove after iteration.
  SmallVector<MachineBasicBlock *, 4> ToRemove;

  for (auto MBI = MF.begin(), MBE = MF.end(); MBI != MBE; ++MBI) {
    // The entry block cannot be a trampoline.
    if (MBI == MF.begin())
      continue;

    MachineBasicBlock &TrampolineMBB = *MBI;

    // Look for a trampoline block: contains only a single JP_nn.
    auto TI = TrampolineMBB.getFirstNonDebugInstr();
    if (TI == TrampolineMBB.end())
      continue;
    if (TI->getOpcode() != Z80::JP_nn)
      continue;
    // Must be the only real instruction.
    if (TrampolineMBB.getLastNonDebugInstr() != TI)
      continue;

    // Must have exactly one predecessor.
    if (TrampolineMBB.pred_size() != 1)
      continue;
    MachineBasicBlock *PredMBB = *TrampolineMBB.pred_begin();

    // Predecessor must be the layout predecessor.
    if (&*std::prev(TrampolineMBB.getIterator()) != PredMBB)
      continue;

    // Predecessor must end with a conditional JR.
    auto PredLast = PredMBB->getLastNonDebugInstr();
    if (PredLast == PredMBB->end())
      continue;
    unsigned JPCondOpc = invertJRtoJP(PredLast->getOpcode());
    if (!JPCondOpc)
      continue;

    // The JR target must be the trampoline's layout successor (the block
    // that the JR "skips to" when the condition is true).
    MachineBasicBlock *JRTarget = PredLast->getOperand(0).getMBB();
    auto AfterTrampoline = std::next(TrampolineMBB.getIterator());
    if (AfterTrampoline == MF.end() || JRTarget != &*AfterTrampoline)
      continue;

    MachineBasicBlock *FarTarget = TI->getOperand(0).getMBB();

    LLVM_DEBUG(dbgs() << "Z80BranchCleanup: collapsing trampoline in "
                      << printMBBReference(TrampolineMBB) << "\n");

    // Replace JR_CC in predecessor with JP_CC to far target.
    DebugLoc DL = PredLast->getDebugLoc();
    PredLast->eraseFromParent();
    BuildMI(*PredMBB, PredMBB->end(), DL, TII->get(JPCondOpc))
        .addMBB(FarTarget);

    // Update predecessor's successor edges:
    // Was: PredMBB → {TrampolineMBB, JRTarget}
    // Now: PredMBB → {FarTarget, JRTarget}  (JRTarget via fallthrough)
    PredMBB->removeSuccessor(&TrampolineMBB);
    PredMBB->addSuccessor(FarTarget);
    // JRTarget is already a successor of PredMBB.

    // Clean up trampoline's successor list before removal.
    TrampolineMBB.removeSuccessor(FarTarget);

    // Remove the JP instruction and mark trampoline for removal.
    TI->eraseFromParent();
    ToRemove.push_back(&TrampolineMBB);
    Changed = true;
  }

  // Remove empty trampoline blocks.
  for (MachineBasicBlock *MBB : ToRemove) {
    MBB->eraseFromParent();
  }

  return Changed;
}

char Z80BranchCleanup::ID = 0;

INITIALIZE_PASS(Z80BranchCleanup, DEBUG_TYPE,
                "Clean up Z80 branch relaxation trampolines", false, false)

MachineFunctionPass *llvm::createZ80BranchCleanupPass() {
  return new Z80BranchCleanup();
}
