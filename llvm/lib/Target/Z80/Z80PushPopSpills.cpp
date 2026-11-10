//===-- Z80PushPopSpills.cpp - Spill to the stack with PUSH and POP -------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Replaces 16-bit spill slots used in stack order within one basic block with
// SPILL_PUSH and SPILL_POP.
//
// This replacement makes spills smaller and faster. A 16-bit spill and reload
// through SP or IX take 12 to 14 bytes. PUSH and POP take 2 bytes.
//
//===----------------------------------------------------------------------===//

#include "Z80PushPopSpills.h"
#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;

#define DEBUG_TYPE "z80-push-pop-spills"

STATISTIC(NumSlots, "Number of spill slots replaced with PUSH and POP");
STATISTIC(NumKeptReloads, "Number of reloads that pop and push again");

static cl::opt<bool>
    EnablePushPopSpills("z80-enable-push-pop-spills", cl::init(true),
                        cl::Hidden,
                        cl::desc("Spill to the stack with PUSH and POP"));

namespace {

class Z80PushPopSpills : public MachineFunctionPass {
public:
  static char ID;

  Z80PushPopSpills() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override { return "Z80 PUSH/POP Spills"; }

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  using SlotSet = DenseSet<int>;

  const TargetInstrInfo *TII = nullptr;
  const TargetRegisterInfo *TRI = nullptr;
  MachineFrameInfo *MFI = nullptr;

  void collectCandidates(MachineFunction &MF,
                         DenseMap<MachineBasicBlock *, SlotSet> &Cands);
  void orderSpills(MachineBasicBlock &MBB, const SlotSet &Cands);
  bool matchStackOrder(MachineBasicBlock &MBB, const SlotSet &Cands,
                       SlotSet &Failed);
  void rewrite(MachineBasicBlock &MBB, const SlotSet &Cands);
};

} // end anonymous namespace

char Z80PushPopSpills::ID = 0;

INITIALIZE_PASS(Z80PushPopSpills, DEBUG_TYPE, "Z80 PUSH/POP Spills", false,
                false)

MachineFunctionPass *llvm::createZ80PushPopSpillsPass() {
  return new Z80PushPopSpills();
}

static bool isSpill(const MachineInstr &MI) {
  unsigned Opc = MI.getOpcode();
  return (Opc == Z80::SPILL_GR16 || Opc == Z80::SPILL_ANY16) &&
         MI.getOperand(1).isFI();
}

static bool isReload(const MachineInstr &MI) {
  unsigned Opc = MI.getOpcode();
  return (Opc == Z80::RELOAD_GR16 || Opc == Z80::RELOAD_ANY16) &&
         MI.getOperand(1).isFI();
}

static Register accessReg(const MachineInstr &MI) {
  return MI.getOperand(0).getReg();
}

static int accessSlot(const MachineInstr &MI) {
  return MI.getOperand(1).getIndex();
}

/// Number of reloads of each slot in \p Cands within \p MBB.
static DenseMap<int, unsigned> countReloads(const MachineBasicBlock &MBB,
                                            const DenseSet<int> &Cands) {
  DenseMap<int, unsigned> Count;
  for (const MachineInstr &MI : MBB)
    if (isReload(MI) && Cands.count(accessSlot(MI)))
      ++Count[accessSlot(MI)];
  return Count;
}

/// Whether \p MI spills or reloads a whole slot.
static bool isPlainAccess(const MachineInstr &MI) {
  return (isSpill(MI) || isReload(MI)) && MI.getOperand(2).getImm() == 0;
}

void Z80PushPopSpills::collectCandidates(
    MachineFunction &MF, DenseMap<MachineBasicBlock *, SlotSet> &Cands) {
  DenseMap<int, MachineBasicBlock *> Home;
  DenseMap<int, unsigned> NumSpills;
  DenseMap<int, unsigned> NumReloads;
  SlotSet Rejected;

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      // A debug value in a replaced slot is dropped after the rewrite.
      if (MI.isDebugInstr())
        continue;
      for (const MachineOperand &MO : MI.operands()) {
        if (!MO.isFI())
          continue;
        int FI = MO.getIndex();
        if (FI < 0 || !MFI->isSpillSlotObjectIndex(FI))
          continue;
        auto [It, Inserted] = Home.try_emplace(FI, &MBB);
        if (!Inserted && It->second != &MBB)
          Rejected.insert(FI);
        if (!isPlainAccess(MI))
          Rejected.insert(FI);
        else if (isSpill(MI))
          ++NumSpills[FI];
        else
          ++NumReloads[FI];
      }
    }
  }

  for (auto [FI, MBB] : Home)
    if (!Rejected.count(FI) && MFI->getObjectSize(FI) == 2 &&
        NumSpills.lookup(FI) == 1 && NumReloads.lookup(FI) > 0)
      Cands[MBB].insert(FI);

  // The spill must precede every reload.
  for (auto &[MBB, Slots] : Cands) {
    SlotSet Stored;
    for (MachineInstr &MI : *MBB) {
      if (isSpill(MI))
        Stored.insert(accessSlot(MI));
      else if (isReload(MI) && !Stored.count(accessSlot(MI)))
        Slots.erase(accessSlot(MI));
    }
  }
}

void Z80PushPopSpills::orderSpills(MachineBasicBlock &MBB,
                                   const SlotSet &Cands) {
  DenseMap<int, unsigned> LastReload;
  unsigned Pos = 0;
  for (MachineInstr &MI : MBB) {
    ++Pos;
    if (isReload(MI))
      LastReload[accessSlot(MI)] = Pos;
  }

  SmallVector<MachineInstr *, 8> Run;
  auto Flush = [&](MachineBasicBlock::iterator InsertPt) {
    if (Run.size() > 1) {
      SmallVector<MachineInstr *, 8> Sorted(Run);
      llvm::stable_sort(Sorted, [&](MachineInstr *A, MachineInstr *B) {
        return LastReload.lookup(accessSlot(*A)) >
               LastReload.lookup(accessSlot(*B));
      });
      // Two spills of one register: the kill moves to the later spill.
      SmallVector<Register, 4> Killed;
      for (MachineInstr *MI : Run) {
        MachineOperand &Src = MI->getOperand(0);
        if (Src.isKill()) {
          Killed.push_back(Src.getReg());
          Src.setIsKill(false);
        }
      }
      // A debug value in the run may name the slot of a later spill. The debug
      // values go after the spills.
      SmallVector<MachineInstr *, 4> Debug;
      for (MachineInstr &MI :
           make_range(MachineBasicBlock::iterator(Run.front()), InsertPt))
        if (MI.isDebugInstr())
          Debug.push_back(&MI);
      for (MachineInstr *MI : Sorted)
        MBB.splice(InsertPt, &MBB, MI->getIterator());
      for (MachineInstr *MI : Debug)
        MBB.splice(InsertPt, &MBB, MI->getIterator());
      for (Register Reg : Killed)
        for (MachineInstr *MI : llvm::reverse(Sorted))
          if (accessReg(*MI) == Reg) {
            MI->getOperand(0).setIsKill(true);
            break;
          }
    }
    Run.clear();
  };

  for (auto I = MBB.begin(), E = MBB.end(); I != E;) {
    MachineInstr &MI = *I++;
    if (MI.isDebugInstr())
      continue;
    if (isSpill(MI) && Cands.count(accessSlot(MI))) {
      Run.push_back(&MI);
      continue;
    }
    Flush(MI.getIterator());
  }
  Flush(MBB.end());
}

/// Simulates the stack through \p MBB and reorders adjacent reloads to match
/// the stack. Slots that are not in stack order go into \p Failed. Returns
/// true if every candidate is in stack order.
bool Z80PushPopSpills::matchStackOrder(MachineBasicBlock &MBB,
                                       const SlotSet &Cands, SlotSet &Failed) {
  DenseMap<int, unsigned> Remaining = countReloads(MBB, Cands);

  // An entry with Slot >= 0 is a candidate slot. An entry with Slot == -1 is a
  // number of bytes pushed by other instructions.
  struct Entry {
    int Slot;
    int Bytes;
  };
  SmallVector<Entry, 8> Stack;

  auto FailAll = [&] {
    for (const Entry &En : Stack)
      if (En.Slot >= 0)
        Failed.insert(En.Slot);
  };
  auto AdjustOther = [&](int Delta) {
    if (Delta > 0) {
      if (!Stack.empty() && Stack.back().Slot < 0)
        Stack.back().Bytes += Delta;
      else
        Stack.push_back({-1, Delta});
      return;
    }
    for (int Need = -Delta; Need > 0;) {
      if (Stack.empty())
        return;
      Entry &Top = Stack.back();
      if (Top.Slot >= 0) {
        FailAll();
        return;
      }
      int Take = std::min(Need, Top.Bytes);
      Top.Bytes -= Take;
      Need -= Take;
      if (Top.Bytes == 0)
        Stack.pop_back();
    }
  };
  auto IsCandidateReload = [&](const MachineInstr &MI) {
    return isReload(MI) && Cands.count(accessSlot(MI));
  };

  for (auto I = MBB.begin(), E = MBB.end(); I != E && Failed.empty(); ++I) {
    if (I->isDebugInstr())
      continue;

    if (isSpill(*I) && Cands.count(accessSlot(*I))) {
      Stack.push_back({accessSlot(*I), 0});
      continue;
    }

    if (IsCandidateReload(*I)) {
      // Move the reload of the top slot to the front of the run of adjacent
      // reloads starting here.
      if (!Stack.empty() && Stack.back().Slot >= 0 &&
          Stack.back().Slot != accessSlot(*I)) {
        SmallVector<MachineInstr *, 4> Run;
        for (auto J = I; J != E; ++J) {
          if (J->isDebugInstr())
            continue;
          if (!IsCandidateReload(*J))
            break;
          Run.push_back(&*J);
        }
        MachineInstr *Top = nullptr;
        for (MachineInstr *MI : Run)
          if (accessSlot(*MI) == Stack.back().Slot) {
            Top = MI;
            break;
          }
        bool Disjoint = true;
        for (unsigned A = 0; A < Run.size() && Disjoint; ++A)
          for (unsigned B = A + 1; B < Run.size() && Disjoint; ++B)
            Disjoint =
                !TRI->regsOverlap(accessReg(*Run[A]), accessReg(*Run[B]));
        if (Top && Disjoint) {
          // A debug value that Top moves past would describe the reloaded
          // register. This debug value becomes undef.
          Register Def = accessReg(*Top);
          for (MachineInstr &DV :
               make_range(I, MachineBasicBlock::iterator(Top)))
            if (DV.isDebugValue() &&
                any_of(DV.debug_operands(), [&](const MachineOperand &MO) {
                  return MO.isReg() && MO.getReg().isValid() &&
                         TRI->regsOverlap(MO.getReg(), Def);
                }))
              DV.setDebugValueUndef();
          MBB.splice(I, &MBB, Top->getIterator());
          I = Top->getIterator();
        }
      }

      int FI = accessSlot(*I);
      if (Stack.empty() || Stack.back().Slot != FI) {
        Failed.insert(FI);
        break;
      }
      if (--Remaining[FI] == 0)
        Stack.pop_back();
      continue;
    }

    if (int Adj = TII->getSPAdjust(*I)) {
      AdjustOther(Adj);
      continue;
    }
    // getSPAdjust counts calls and frame pseudos.
    if (!TII->isFrameInstr(*I) && !I->isCall() &&
        (I->readsRegister(Z80::SP, TRI) || I->modifiesRegister(Z80::SP, TRI)))
      FailAll();
  }

  if (!Failed.empty())
    return false;
  FailAll();
  return Failed.empty();
}

void Z80PushPopSpills::rewrite(MachineBasicBlock &MBB, const SlotSet &Cands) {
  DenseMap<int, unsigned> Remaining = countReloads(MBB, Cands);

  for (MachineInstr &MI : llvm::make_early_inc_range(MBB)) {
    if (!(isSpill(MI) || isReload(MI)) || !Cands.count(accessSlot(MI)))
      continue;
    const MachineOperand &RegOp = MI.getOperand(0);
    Register Reg = RegOp.getReg();
    const DebugLoc &DL = MI.getDebugLoc();
    if (isSpill(MI)) {
      BuildMI(MBB, MI, DL, TII->get(Z80::SPILL_PUSH))
          .addReg(Reg, getKillRegState(RegOp.isKill()) |
                           getUndefRegState(RegOp.isUndef()));
    } else {
      bool Last = --Remaining[accessSlot(MI)] == 0;
      // A reload before the last reload pushes the register again.
      BuildMI(MBB, MI, DL, TII->get(Z80::SPILL_POP))
          .addReg(Reg,
                  RegState::Define | getDeadRegState(Last && RegOp.isDead()));
      if (!Last) {
        BuildMI(MBB, MI, DL, TII->get(Z80::SPILL_PUSH)).addReg(Reg);
        ++NumKeptReloads;
      }
    }
    MI.eraseFromParent();
  }

  for (int FI : Cands)
    MFI->RemoveStackObject(FI);
  NumSlots += Cands.size();
}

bool Z80PushPopSpills::runOnMachineFunction(MachineFunction &MF) {
  if (!EnablePushPopSpills || skipFunction(MF.getFunction()))
    return false;

  // longjmp restores SP but not the bytes popped since setjmp.
  // TODO: Check MF.exposesReturnsTwice() once GlobalISel sets this flag.
  if (MF.getFunction().callsFunctionThatReturnsTwice())
    return false;

  TII = MF.getSubtarget().getInstrInfo();
  TRI = MF.getSubtarget().getRegisterInfo();
  MFI = &MF.getFrameInfo();

  DenseMap<MachineBasicBlock *, SlotSet> Cands;
  collectCandidates(MF, Cands);

  bool Changed = false;
  for (MachineBasicBlock &MBB : MF) {
    auto It = Cands.find(&MBB);
    if (It == Cands.end() || It->second.empty())
      continue;
    SlotSet &Slots = It->second;
    orderSpills(MBB, Slots);
    // Each failed round drops at least one slot.
    for (SlotSet Failed; !Slots.empty() && !matchStackOrder(MBB, Slots, Failed);
         Failed.clear())
      for (int FI : Failed)
        Slots.erase(FI);
    if (Slots.empty())
      continue;
    rewrite(MBB, Slots);
    Changed = true;
  }

  if (!Changed)
    return false;

  // Debug values in replaced slots lose the location.
  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB) {
      if (!MI.isDebugValue())
        continue;
      for (MachineOperand &MO : MI.debug_operands()) {
        if (!MO.isFI() || !MFI->isDeadObjectIndex(MO.getIndex()))
          continue;
        MO.ChangeToRegister(Register(), /*isDef=*/false);
        if (MI.isNonListDebugValue() && MI.getDebugOffset().isImm())
          MI.getDebugOffset().ChangeToRegister(Register(), /*isDef=*/false);
      }
    }
  return true;
}
