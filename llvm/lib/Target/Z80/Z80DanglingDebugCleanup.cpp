//===-- Z80DanglingDebugCleanup.cpp - Z80 dangling debug cleanup ----------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Z80DanglingDebugCleanup.h"

#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/PassRegistry.h"

#include "Z80.h"

#define DEBUG_TYPE "z80-dangling-debug-cleanup"

using namespace llvm;

namespace {

// The legalizer can erase the last definition of a wide value (a scalarized
// vector, a split integer) while a debug operand still refers to it.
// RegBankSelect maps debug operands like any other, and no 8/16-bit bank can
// carry the wide dangling type. The value no longer exists, so mark it as
// unavailable instead.
class Z80DanglingDebugCleanup : public MachineFunctionPass {
public:
  static char ID;
  Z80DanglingDebugCleanup() : MachineFunctionPass(ID) {
    llvm::initializeZ80DanglingDebugCleanupPass(
        *PassRegistry::getPassRegistry());
  }
  StringRef getPassName() const override {
    return "Z80 dangling debug value cleanup";
  }
  bool runOnMachineFunction(MachineFunction &MF) override {
    MachineRegisterInfo &MRI = MF.getRegInfo();
    bool Changed = false;
    auto IsDangling = [&](const MachineOperand &MO) {
      return MO.isReg() && MO.getReg().isVirtual() &&
             !MRI.getVRegDef(MO.getReg());
    };
    for (MachineBasicBlock &MBB : MF)
      for (MachineInstr &MI : MBB) {
        // Only DBG_VALUE / DBG_VALUE_LIST / DBG_INSTR_REF carry value operands;
        // debug_operands() asserts isDebugValueLike(). DBG_LABEL / DBG_PHI are
        // debug instrs too but have NO value operands -- calling debug_operands()
        // on them drop_front(2)s a shorter operand list into an out-of-bounds
        // range, and in a release (no-assert) build the garbage iteration
        // segfaults in setReg (crashed rcbios @rwoper / @bios_write_c DBG_LABEL).
        if (!MI.isDebugValueLike() || none_of(MI.debug_operands(), IsDangling))
          continue;
        // A location with an unavailable operand cannot be evaluated at all,
        // so the canonical form marks the whole value undef.
        if (MI.isDebugValue())
          MI.setDebugValueUndef();
        else
          for (MachineOperand &MO : MI.debug_operands())
            if (IsDangling(MO))
              MO.setReg(Register());
        Changed = true;
      }
    return Changed;
  }
};

} // namespace

char Z80DanglingDebugCleanup::ID = 0;

INITIALIZE_PASS(Z80DanglingDebugCleanup, DEBUG_TYPE,
                "Z80 dangling debug value cleanup", false, false)

MachineFunctionPass *llvm::createZ80DanglingDebugCleanupPass() {
  return new Z80DanglingDebugCleanup();
}
