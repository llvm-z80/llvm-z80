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
// This pass removes OR A when a preceding instruction has already left every
// flag exactly as OR A would.
//
//===----------------------------------------------------------------------===//

#include "Z80PostRACompareMerge.h"
#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80InstrInfo.h"
#include "Z80Subtarget.h"

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

/// Whether Z, S and P/V describe the value now sitting in A after \p MI.
///
/// This is the question OR A answers, so an instruction on this list has
/// already answered it. What is deliberately absent:
///
///   - The comparisons report Z for A minus their operand, not for A. Reading
///     one would turn "if (a == 0)" into "if (a == operand)".
///   - CPL and the accumulator rotates RLCA/RRCA/RLA/RRA write A and the flags
///     together but leave Z untouched, so Z still describes whatever ran
///     before them.
///   - POP AF writes A and the flags together, but its Z is whatever was
///     pushed rather than anything about the A it just loaded.
static bool setsZeroFlagFromA(const MachineInstr &MI) {
  switch (MI.getOpcode()) {
  case Z80::AND_r:
  case Z80::AND_n:
  case Z80::AND_HLind:
  case Z80::AND_IXd:
  case Z80::AND_IYd:
  case Z80::OR_r:
  case Z80::OR_n:
  case Z80::OR_HLind:
  case Z80::OR_IXd:
  case Z80::OR_IYd:
  case Z80::XOR_r:
  case Z80::XOR_n:
  case Z80::XOR_HLind:
  case Z80::XOR_IXd:
  case Z80::XOR_IYd:
  case Z80::ADD_A_r:
  case Z80::ADD_A_n:
  case Z80::ADD_A_HLind:
  case Z80::ADD_A_IXd:
  case Z80::ADD_A_IYd:
  case Z80::ADC_A_r:
  case Z80::ADC_A_n:
  case Z80::ADC_A_HLind:
  case Z80::ADC_A_IXd:
  case Z80::ADC_A_IYd:
  case Z80::SUB_r:
  case Z80::SUB_n:
  case Z80::SUB_HLind:
  case Z80::SUB_IXd:
  case Z80::SUB_IYd:
  case Z80::SBC_A_r:
  case Z80::SBC_A_n:
  case Z80::SBC_A_HLind:
  case Z80::SBC_A_IXd:
  case Z80::SBC_A_IYd:
  case Z80::NEG:
    return true;
  case Z80::INC_r:
  case Z80::DEC_r:
    return MI.getOperand(0).getReg() == Z80::A;
  default:
    return false;
  }
}

/// Whether \p MI also leaves the carry clear, which is the other half of what
/// OR A does. The logical operations do; the arithmetic ones leave a carry
/// that means something, and INC/DEC leave the carry alone entirely.
static bool clearsCarry(const MachineInstr &MI) {
  switch (MI.getOpcode()) {
  case Z80::AND_r:
  case Z80::AND_n:
  case Z80::AND_HLind:
  case Z80::AND_IXd:
  case Z80::AND_IYd:
  case Z80::OR_r:
  case Z80::OR_n:
  case Z80::OR_HLind:
  case Z80::OR_IXd:
  case Z80::OR_IYd:
  case Z80::XOR_r:
  case Z80::XOR_n:
  case Z80::XOR_HLind:
  case Z80::XOR_IXd:
  case Z80::XOR_IYd:
    return true;
  default:
    return false;
  }
}

/// Whether \p MI reads the flags and asks only about Z.
static bool testsOnlyZeroFlag(const MachineInstr &MI) {
  switch (MI.getOpcode()) {
  case Z80::JP_Z_nn:
  case Z80::JP_NZ_nn:
  case Z80::JR_Z_e:
  case Z80::JR_NZ_e:
  case Z80::CALL_Z_nn:
  case Z80::CALL_NZ_nn:
  case Z80::RET_Z:
  case Z80::RET_NZ:
    return true;
  default:
    return false;
  }
}

/// Whether the carry that OR A would have cleared is read before something
/// writes the flags again. \p MI is the OR A under consideration.
static bool carryIsReadAfter(MachineBasicBlock::iterator MI,
                             MachineBasicBlock &MBB,
                             const TargetRegisterInfo *TRI) {
  for (auto I = std::next(MI), E = MBB.end(); I != E; ++I) {
    if (I->readsRegister(Z80::FLAGS, TRI) && !testsOnlyZeroFlag(*I))
      return true;
    if (I->isInlineAsm())
      return true;
    if (I->definesRegister(Z80::FLAGS, TRI))
      return false;
  }
  return Z80::isLiveAt(MBB, MBB.end(), Z80::FLAGS, TRI);
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
  const TargetRegisterInfo *TRI =
      MF.getSubtarget<Z80Subtarget>().getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    // The instruction that left Z describing what A currently holds, if one
    // has run since the last thing that disturbed either.
    const MachineInstr *ZSource = nullptr;
    SmallVector<MachineInstr *, 4> ToErase;

    for (MachineInstr &MI : MBB) {
      // A debug instruction stands between nothing, and letting it clear the
      // state below would make -g change the code that comes out.
      if (MI.isDebugInstr())
        continue;

      // OR A re-tests what A already holds and clears the carry. Where
      // something has already said the same about Z, the instruction is
      // redundant as long as nobody wanted the carry it clears.
      if (MI.getOpcode() == Z80::OR_r && MI.getOperand(0).getReg() == Z80::A &&
          ZSource &&
          (clearsCarry(*ZSource) || !carryIsReadAfter(MI, MBB, TRI))) {
        LLVM_DEBUG(dbgs() << "  Removing redundant: " << MI);
        ToErase.push_back(&MI);
        continue;
      }

      if (MI.isCall() || MI.isReturn() || MI.isInlineAsm() ||
          MI.isBranch() || MI.isPseudo()) {
        ZSource = nullptr;
        continue;
      }

      if (definesFlags(MI)) {
        ZSource = setsZeroFlagFromA(MI) ? &MI : nullptr;
        continue;
      }

      if (modifiesAWithoutFlags(MI))
        ZSource = nullptr;
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
