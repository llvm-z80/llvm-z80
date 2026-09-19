//===-- Z80SplitDjnzCounters.cpp - Split DJNZ-loop counter live ranges ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// In nested or sequential countdown loops, the register coalescer merges
// entry-edge PHI COPYs into the loop-counter virtual registers. This extends
// outer or earlier counter live ranges across subsequent loops, causing
// greedy register allocation to allocate B to the outer loop or the first
// loop only.
//
// Worked example (from djnz-nested.ll @nested_djnz):
//
//   outer:                     ; preheader for inner loop
//     %0:gr8 = phi %m, %o.next
//   inner:                     ; self-looping MBB
//     %1:gr8 = phi %n, %inner
//     $a = COPY %1:gr8
//     DEC_r $a
//     %1:gr8 = COPY $a
//     JR_NZ_e %bb.inner
//   outer.latch:
//     DEC_r %0:gr8
//     JR_NZ_e %bb.outer
//
// Without splitting: %0's live range spans from entry across the entire inner
// loop down to outer.latch. Greedy regalloc allocates B to %0 first, leaving
// the inner (hot) counter %1 with C or another register. The inner loop
// executes N*M times emitting "DEC C; JR NZ" (16 T-states, 3 bytes), while
// the outer loop executes M times emitting "DJNZ" (13 T-states, 2 bytes).
//
// With splitting: this pass recognizes %bb.inner as a self-loop NZ countdown
// and introduces a preheader copy with a single-register class BReg:
//
//   outer:
//     %2:breg = COPY %1:gr8   ; new virtual register in BReg
//   inner:
//     $a = COPY %2:breg
//     DEC_r $a
//     %2:breg = COPY $a
//     JR_NZ_e %bb.inner
//
// Because %2 is constrained to BReg, greedy regalloc is forced to assign B
// to the inner loop counter %2. The outer counter %0 is assigned C.
// The inner loop emits DJNZ (13 T, 2 B), saving 3 T-states and 1 byte per
// inner iteration.
//
//===----------------------------------------------------------------------===//

#include "Z80SplitDjnzCounters.h"
#include "Z80.h"
#include "Z80InstrInfo.h"
#include "Z80Subtarget.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;

#define DEBUG_TYPE "z80-split-djnz-counters"

static cl::opt<bool> EnableSplitDjnzCounters(
    "z80-split-djnz-counters",
    cl::desc("Split DJNZ loop counter live ranges to prefer B in nested/sequential loops"),
    cl::init(true), cl::Hidden);

namespace {

class Z80SplitDjnzCounters : public MachineFunctionPass {
public:
  static char ID;

  Z80SplitDjnzCounters() : MachineFunctionPass(ID) {
    initializeZ80SplitDjnzCountersPass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override {
    return "Z80 Split DJNZ-loop counter live ranges";
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
};

} // end anonymous namespace

char Z80SplitDjnzCounters::ID = 0;

INITIALIZE_PASS(Z80SplitDjnzCounters, DEBUG_TYPE,
                "Z80 Split DJNZ-loop counter live ranges", false, false)

static Register findCounter16VReg(MachineBasicBlock &MBB,
                                  const MachineRegisterInfo &MRI) {
  for (auto It = MBB.begin(), E = MBB.end(); It != E; ++It) {
    if (It->getOpcode() != Z80::DEC_rr)
      continue;
    if (It->getNumOperands() < 2)
      continue;
    if (!It->getOperand(0).isReg() || !It->getOperand(1).isReg())
      continue;
    Register Src = It->getOperand(1).getReg();
    if (!Src.isVirtual())
      continue;
    if (!Z80::GR16RegClass.hasSubClassEq(MRI.getRegClass(Src)))
      continue;
    return Src;
  }
  return Register();
}

static Register findCounterVReg(MachineBasicBlock &MBB,
                                const MachineRegisterInfo &MRI) {
  for (auto It = MBB.begin(), E = MBB.end(); It != E; ++It) {
    if (It->getOpcode() != TargetOpcode::COPY)
      continue;
    if (!It->getOperand(0).isReg() ||
        It->getOperand(0).getReg() != Z80::A)
      continue;
    if (!It->getOperand(1).isReg())
      continue;
    Register V = It->getOperand(1).getReg();
    if (!V.isVirtual())
      continue;
    if (!Z80::GR8RegClass.hasSubClassEq(MRI.getRegClass(V)))
      continue;
    auto J = std::next(It);
    while (J != E && J->isMetaInstruction())
      ++J;
    if (J == E || J->getOpcode() != Z80::DEC_r)
      continue;
    if (J->getNumOperands() < 1 || !J->getOperand(0).isReg() ||
        J->getOperand(0).getReg() != Z80::A)
      continue;
    for (auto K = std::next(J); K != E; ++K) {
      if (K->getOpcode() != TargetOpcode::COPY)
        continue;
      if (!K->getOperand(0).isReg() ||
          K->getOperand(0).getReg() != V)
        continue;
      if (!K->getOperand(1).isReg() ||
          K->getOperand(1).getReg() != Z80::A)
        continue;
      return V;
    }
  }
  return Register();
}

static bool isSelfBackEdgeNZLoop(const MachineBasicBlock &MBB) {
  for (const MachineInstr &Term : MBB.terminators()) {
    unsigned Opc = Term.getOpcode();
    if (Opc != Z80::JR_NZ_e && Opc != Z80::JP_NZ_nn)
      continue;
    if (Term.getNumOperands() == 0 || !Term.getOperand(0).isMBB())
      continue;
    if (Term.getOperand(0).getMBB() == &MBB)
      return true;
  }
  return false;
}

static MachineBasicBlock *findUniquePreheader(MachineBasicBlock &MBB) {
  MachineBasicBlock *Found = nullptr;
  for (MachineBasicBlock *Pred : MBB.predecessors()) {
    if (Pred == &MBB)
      continue;
    if (Found)
      return nullptr;
    Found = Pred;
  }
  return Found;
}

static bool hasAnyRefOutsideMBB(Register Reg, MachineBasicBlock &MBB,
                                const MachineRegisterInfo &MRI) {
  for (const MachineInstr &MI : MRI.reg_nodbg_instructions(Reg)) {
    if (MI.getParent() != &MBB)
      return true;
  }
  return false;
}

static bool hasUseOutsideMBB(Register Reg, MachineBasicBlock &MBB,
                             const MachineRegisterInfo &MRI) {
  for (const MachineInstr &MI : MRI.use_nodbg_instructions(Reg)) {
    if (MI.getParent() != &MBB)
      return true;
  }
  return false;
}

static bool splitCounterAt(MachineBasicBlock &MBB, Register Counter,
                           const TargetRegisterClass &TargetRC,
                           MachineRegisterInfo &MRI,
                           const TargetInstrInfo &TII) {
  // If the counter is purely local to MBB, it is already contained.
  if (!hasAnyRefOutsideMBB(Counter, MBB, MRI))
    return false;
  // If the counter value escapes after the loop, replacing MBB uses alone is unsound.
  if (hasUseOutsideMBB(Counter, MBB, MRI))
    return false;

  // Need a unique preheader to safely host the pre-loop copy.
  MachineBasicBlock *Preheader = findUniquePreheader(MBB);
  if (!Preheader)
    return false;

  Register NewCounter = MRI.createVirtualRegister(&TargetRC);

  auto InsertBefore = Preheader->getFirstTerminator();
  DebugLoc DL = (InsertBefore != Preheader->end())
                    ? InsertBefore->getDebugLoc()
                    : DebugLoc();
  BuildMI(*Preheader, InsertBefore, DL, TII.get(TargetOpcode::COPY),
          NewCounter)
      .addReg(Counter);

  for (MachineInstr &MI : MBB) {
    for (MachineOperand &MO : MI.operands()) {
      if (MO.isReg() && MO.getReg() == Counter)
        MO.setReg(NewCounter);
    }
  }
  return true;
}

bool Z80SplitDjnzCounters::runOnMachineFunction(MachineFunction &MF) {
  if (!EnableSplitDjnzCounters)
    return false;

  // Skip live-range splitting when optimizing for code size (-Os / -Oz).
  //
  // WHAT: Avoid splitting DJNZ counter live ranges if the function has the
  // `optsize` or `minsize` attribute.
  //
  // WHY: Splitting inserts a preheader copy (e.g. `ld b, l`) inside the outer
  // loop body. In nested loops this trades code size for execution speed:
  //   - Without splitting (size-optimized): outer loop gets B (DJNZ, 2 B) and
  //     inner gets C (DEC C; JR NZ, 3 B). Total binary size = 78 bytes.
  //   - With splitting (speed-optimized): preheader copy `ld b, l` is inserted,
  //     inner gets B (DJNZ, 2 B) and outer gets C. Total binary size = 82 bytes.
  // For -Os and -Oz, every byte counts, so we prefer the 78-byte layout.
  //
  // GOTCHA: Do not check CodeGenOptLevel here because -Os, -Oz, and -O2 all
  // share CodeGenOptLevel::Default; the function-level `hasOptSize()` attribute
  // is the canonical discriminator.
  if (MF.getFunction().hasOptSize())
    return false; // do not insert preheader copies under -Os / -Oz

  const auto &STI = MF.getSubtarget<Z80Subtarget>();
  if (!STI.hasZ80())
    return false;

  MachineRegisterInfo &MRI = MF.getRegInfo();
  const TargetInstrInfo &TII = *STI.getInstrInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    if (!isSelfBackEdgeNZLoop(MBB))
      continue;

    if (Register Counter = findCounterVReg(MBB, MRI)) {
      if (splitCounterAt(MBB, Counter, Z80::BRegRegClass, MRI, TII)) {
        Changed = true;
        continue;
      }
    }

    if (Register Counter16 = findCounter16VReg(MBB, MRI)) {
      if (splitCounterAt(MBB, Counter16, Z80::BCRegRegClass, MRI, TII)) {
        Changed = true;
        continue;
      }
    }
  }

  return Changed;
}

MachineFunctionPass *llvm::createZ80SplitDjnzCountersPass() {
  return new Z80SplitDjnzCounters;
}
