//===-- Z80PostRACompareMerge.cpp - Remove redundant flag-setting ops ------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// After register allocation, the backend often emits redundant OR A
// instructions to set the Z flag before a conditional branch, even when the
// preceding ALU instruction already set the Z flag correctly.
//
// Example:
//   xor e        ; already sets Z flag
//   or a         ; redundant — removed by this pass
//   jr z, .label
//
// This pass removes OR_A when the Z flag is already valid from a preceding
// instruction that defines FLAGS and operates on A.
//
//===----------------------------------------------------------------------===//

#include "Z80PostRACompareMerge.h"
#include "MCTargetDesc/Z80MCTargetDesc.h"

#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "z80-post-ra-compare-merge"

class Z80PostRACompareMerge : public MachineFunctionPass {
public:
  static char ID;
  Z80PostRACompareMerge() : MachineFunctionPass(ID) {}
  StringRef getPassName() const override { return "Z80PostRACompareMerge"; }
  bool runOnMachineFunction(MachineFunction &MF) override;
};

char Z80PostRACompareMerge::ID = 0;

INITIALIZE_PASS(Z80PostRACompareMerge, DEBUG_TYPE,
                "Z80 Post-RA Redundant Compare Removal", false, false)

/// Returns true if MI defines the FLAGS register.
static bool definesFlags(const MachineInstr &MI) {
  for (const MachineOperand &MO : MI.operands()) {
    if (MO.isReg() && MO.isDef() && MO.getReg() == Z80::FLAGS)
      return true;
  }
  if (MI.getDesc().hasImplicitDefOfPhysReg(Z80::FLAGS))
    return true;
  return false;
}

/// Returns true if MI modifies the A register without setting FLAGS.
static bool modifiesAWithoutFlags(const MachineInstr &MI) {
  bool ModifiesA = false;
  for (const MachineOperand &MO : MI.operands()) {
    if (MO.isReg() && MO.isDef() && MO.getReg() == Z80::A)
      ModifiesA = true;
  }
  if (MI.getDesc().hasImplicitDefOfPhysReg(Z80::A))
    ModifiesA = true;
  return ModifiesA && !definesFlags(MI);
}

bool Z80PostRACompareMerge::runOnMachineFunction(MachineFunction &MF) {
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    bool ZFlagValid = false;
    SmallVector<MachineInstr *, 4> ToErase;

    for (MachineInstr &MI : MBB) {
      if (MI.getOpcode() == Z80::OR_A && ZFlagValid) {
        LLVM_DEBUG(dbgs() << "  Removing redundant: " << MI);
        ToErase.push_back(&MI);
        continue;
      }

      if (MI.isCall() || MI.isReturn() || MI.isInlineAsm() ||
          MI.isBranch() || MI.isPseudo()) {
        ZFlagValid = false;
        continue;
      }

      if (definesFlags(MI)) {
        // Check if this instruction also operates on A, meaning Z
        // reflects A's value. OR_A tests A, so we can only elide it
        // when the preceding instruction set Z for A specifically.
        bool SetsZForA = false;
        for (const MachineOperand &MO : MI.operands()) {
          if (MO.isReg() && MO.isDef() && MO.getReg() == Z80::A) {
            SetsZForA = true;
            break;
          }
        }
        if (MI.getDesc().hasImplicitDefOfPhysReg(Z80::A))
          SetsZForA = true;
        // CP doesn't def A but sets Z based on A's comparison.
        if (MI.getOpcode() == Z80::CP_r || MI.getOpcode() == Z80::CP_n ||
            MI.getOpcode() == Z80::CP_HLind || MI.getOpcode() == Z80::CP_IXd)
          SetsZForA = true;

        ZFlagValid = SetsZForA;
        continue;
      }

      if (modifiesAWithoutFlags(MI))
        ZFlagValid = false;
    }

    for (MachineInstr *MI : ToErase) {
      MI->eraseFromParent();
      Changed = true;
    }
  }

  return Changed;
}

MachineFunctionPass *llvm::createZ80PostRACompareMerge() {
  return new Z80PostRACompareMerge();
}
