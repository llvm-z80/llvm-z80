//===-- Z80AccumulatorCopies.cpp - Isolate accumulator operands -----------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The 8-bit ALU takes its operand and leaves its result in A, which selection
// models as operands in Ac, reached through copies of registers of their own.
// The machine SSA passes fold those copies away: copy propagation in
// MachineCSE hands the instruction the value itself and narrows the value's
// class to Ac for all of its range. Two such values alive at once can then
// only be allocated by splitting one of them, and the split lands wherever
// the allocator finds a register rather than where the value was going.
//
// This pass puts the copies back, next to the instruction, and widens the
// value's class to what its other operands allow. The same goes for a value
// whose accumulator operand is gone altogether, as when a zero test is
// dropped for the flags its value already set. Whether a value stays in A is
// then the coalescer's decision, which Z80RegisterInfo::shouldCoalesce takes
// only where nothing else needs A meanwhile.
//
//===----------------------------------------------------------------------===//

#include "Z80AccumulatorCopies.h"
#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80.h"
#include "Z80RegisterInfo.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"

using namespace llvm;

#define DEBUG_TYPE "z80-acc-copies"

STATISTIC(NumCopies, "Number of copies inserted around accumulator operands");

namespace {

class Z80AccumulatorCopies : public MachineFunctionPass {
public:
  static char ID;

  Z80AccumulatorCopies() : MachineFunctionPass(ID) {
    initializeZ80AccumulatorCopiesPass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override {
    return "Z80 Accumulator Copy Insertion";
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
};

} // end anonymous namespace

char Z80AccumulatorCopies::ID = 0;

INITIALIZE_PASS(Z80AccumulatorCopies, DEBUG_TYPE,
                "Z80 Accumulator Copy Insertion", false, false)

/// Whether \p MO already has a register of its own: a use read from a copy
/// just before it, or a def read only by a copy just after it.
static bool isIsolated(const MachineOperand &MO,
                       const MachineRegisterInfo &MRI) {
  Register Reg = MO.getReg();
  if (!MRI.hasOneNonDBGUse(Reg))
    return false;
  const MachineInstr &MI = *MO.getParent();
  if (MO.isDef()) {
    auto Next = next_nodbg(MI.getIterator(), MI.getParent()->instr_end());
    return Next != MI.getParent()->instr_end() && Next->isCopy() &&
           &*Next == &*MRI.use_instr_nodbg_begin(Reg);
  }
  if (MI.getIterator() == MI.getParent()->instr_begin())
    return false;
  auto Prev = prev_nodbg(MI.getIterator(), MI.getParent()->instr_begin());
  return Prev->isCopy() && Prev->getOperand(0).getReg() == Reg;
}

bool Z80AccumulatorCopies::runOnMachineFunction(MachineFunction &MF) {
  if (skipFunction(MF.getFunction()))
    return false;

  MachineRegisterInfo &MRI = MF.getRegInfo();
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : make_early_inc_range(MBB)) {
      for (unsigned Idx = 0, E = MI.getNumExplicitOperands(); Idx != E; ++Idx) {
        MachineOperand &MO = MI.getOperand(Idx);
        if (!MO.isReg() || !MO.getReg().isVirtual() || MO.isUndef() ||
            MO.isDead() ||
            TII.getRegClass(MI.getDesc(), Idx) != &Z80::AcRegClass ||
            isIsolated(MO, MRI))
          continue;
        Register Reg = MO.getReg();
        Register Own = MRI.createVirtualRegister(&Z80::AcRegClass);
        if (MO.isDef()) {
          BuildMI(MBB, std::next(MI.getIterator()), MI.getDebugLoc(),
                  TII.get(TargetOpcode::COPY), Reg)
              .addReg(Own);
        } else {
          BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Own)
              .addReg(Reg, getKillRegState(MO.isKill()));
          MO.setIsKill(false);
        }
        MO.setReg(Own);
        ++NumCopies;
        Changed = true;
      }
    }
  }

  // Every register left in Ac without an accumulator operand to answer for,
  // whether its copies were put back above or the operand that narrowed it
  // is gone, gets back the class its remaining operands allow.
  for (unsigned I = 0, E = MRI.getNumVirtRegs(); I != E; ++I) {
    Register Reg = Register::index2VirtReg(I);
    if (!MRI.reg_nodbg_empty(Reg) &&
        MRI.getRegClassOrNull(Reg) == &Z80::AcRegClass)
      Changed |= MRI.recomputeRegClass(Reg);
  }
  return Changed;
}

MachineFunctionPass *llvm::createZ80AccumulatorCopiesPass() {
  return new Z80AccumulatorCopies();
}
