//===-- Z80FixupImplicitDefs.cpp - Fix misplaced implicit-defs ------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Two core behaviours leave definitions on instructions that do not make them.
// Both are corrected here rather than at each reader, since every consumer of
// liveness would otherwise have to know about them.
//
//===----------------------------------------------------------------------===//
//
// 1. A super-register definition on a sub-register write
//
// Mitigation for LLVM bug: https://github.com/llvm/llvm-project/issues/156428
//
// Background
// ----------
// The LiveVariables pass (HandlePhysRegUse / FindLastPartialDef) adds
// `implicit-def $hl` to instructions that only define a sub-register
// such as L.  This is an LLVM core bug that affects any backend where
// sub-registers of a register pair are independent (i.e. writing one
// half does NOT clobber the other).  On Z80, writing L does NOT modify
// H -- the two halves of every register pair are independent.
//
// Why this matters
// ----------------
// MachineCopyPropagation (MCP) collects every `isDef()` operand into a
// Defs list and calls `clobberRegister()` for each one.  When MCP sees:
//
//   $h = COPY $e                    ; MCP tracks this copy
//   LD_L_n 0, implicit-def $l, implicit-def $hl
//
// it treats HL as clobbered, which overlaps with H, so it removes the
// $h copy as dead.  This is a miscompilation -- H still holds the value
// from COPY.  The same pattern affects DE (275 instances) and HL (19
// instances) across the test suite.
//
// What this pass does
// -------------------
// For each instruction, we collect the "intended" sub-register defs:
// MCInstrDesc static implicit defs and explicit physical register def
// operands that have a super-register.  If a runtime-added implicit-def
// is a super-register of any intended sub-register def, we remove it.
// This tells MCP that only the sub-register is defined, not the pair.
//
// The pass runs once before the first MCP (via addPostRewrite), which is
// sufficient because no later pass re-adds these implicit-defs.
//
//===----------------------------------------------------------------------===//
//
// 2. A sub-register definition moved onto the last instruction of a copy
//
// Background
// ----------
// TargetInstrInfo::lowerCopy expands a COPY through the target's copyPhysReg
// and then hands every implicit operand of the COPY to whichever instruction
// came out last (transferImplicitOperands).  A pair copy becomes two byte
// moves here, so a COPY that carries the halves as implicit defs
//
//   dead $de = COPY $hl, implicit-def $e, implicit-def $d
//
// becomes
//
//   LD_E_L implicit-def $e, implicit $l
//   LD_D_H implicit-def $d, implicit $h, implicit-def $e, implicit-def $d
//
// where the second move claims to write E, which the first one wrote.
//
// Why this matters
// ----------------
// Backward liveness takes that claim literally and concludes that the value
// in E ends before the second move, so a peephole that borrows a register
// there is free to take E and destroy what the first move put in it.  The
// shape is not avoidable in the target: storing a halfword takes HL for the
// address, so the value moves to another pair whose halves the two byte
// stores then name one at a time.
//
// What this pass does
// -------------------
// A definition the instruction's description does not declare, and which does
// not overlap what it does declare, is not made by that instruction.  Such an
// operand is removed when the instruction before it declares the same
// definition, which is where the write happens.  That condition is what keeps
// a deliberately undefined half: a byte widened to a pair leaves the upper
// half unwritten and says so with an implicit def that nothing else makes, and
// removing that one would leave the pair with no definition at all.
//
// This runs again after post-RA pseudo expansion, which is where copies are
// lowered.
//
// Both mitigations should be removed once the upstream behaviour changes.
//
//===----------------------------------------------------------------------===//

#include "Z80FixupImplicitDefs.h"
#include "Z80.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/MC/MCInstrDesc.h"

using namespace llvm;

#define DEBUG_TYPE "z80-fixup-implicit-defs"

namespace {

class Z80FixupImplicitDefs : public MachineFunctionPass {
public:
  static char ID;

  Z80FixupImplicitDefs() : MachineFunctionPass(ID) {
    initializeZ80FixupImplicitDefsPass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override {
    return "Z80 Fixup Super-Register Implicit Defs";
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties().setNoVRegs();
  }
};

} // end anonymous namespace

char Z80FixupImplicitDefs::ID = 0;

INITIALIZE_PASS(Z80FixupImplicitDefs, DEBUG_TYPE,
                "Z80 Fixup Super-Register Implicit Defs", false, false)

/// Collect the set of physical sub-registers that the instruction is
/// designed to define.  This includes:
///  - MCInstrDesc static implicit defs (e.g. L from LD_L_n)
///  - Explicit physical register def operands (e.g. $l from COPY)
/// We exclude super-register defs (HL, DE, etc.) because those are
/// exactly what we want to remove if added spuriously by LiveVariables.
static SmallSet<MCPhysReg, 4>
getIntendedSubRegDefs(const MachineInstr &MI, const TargetRegisterInfo &TRI) {
  SmallSet<MCPhysReg, 4> Defs;

  // Static implicit defs from MCInstrDesc.
  for (MCPhysReg Reg : MI.getDesc().implicit_defs())
    Defs.insert(Reg);

  // Explicit physical register def operands (e.g. from COPY after
  // VirtRegRewriter).  Only collect sub-registers of register pairs,
  // not the pairs themselves — we want to detect when a sub-register
  // def has a spurious super-register implicit-def attached.
  for (const MachineOperand &MO : MI.operands()) {
    if (!MO.isReg() || !MO.isDef() || MO.isImplicit())
      continue;
    Register Reg = MO.getReg();
    if (Reg.isPhysical() && TRI.superregs(Reg.asMCReg()).begin() !=
                                TRI.superregs(Reg.asMCReg()).end())
      Defs.insert(Reg.asMCReg());
  }

  return Defs;
}

/// Whether \p MI declares that it writes \p Reg, either in its description or
/// through an explicit definition operand.
static bool declaresDef(const MachineInstr &MI, MCPhysReg Reg,
                        const TargetRegisterInfo &TRI) {
  for (MCPhysReg Def : MI.getDesc().implicit_defs())
    if (TRI.regsOverlap(Def, Reg))
      return true;
  for (const MachineOperand &MO : MI.operands())
    if (MO.isReg() && MO.isDef() && !MO.isImplicit() &&
        MO.getReg().isPhysical() && TRI.regsOverlap(MO.getReg(), Reg))
      return true;
  return false;
}

/// Drop definitions that copy lowering moved here from the instruction that
/// makes them. See mitigation 2 at the top of this file.
static bool removeRelocatedDefs(MachineBasicBlock &MBB,
                                const TargetRegisterInfo &TRI) {
  bool Changed = false;
  for (MachineInstr &MI : MBB) {
    // A call or inline assembly names clobbers its description cannot, and a
    // pseudo stands for a sequence that has not been written yet.
    if (MI.isCall() || MI.isInlineAsm() || MI.getDesc().isPseudo())
      continue;
    const MachineInstr *Prev = MI.getPrevNode();
    if (!Prev)
      continue;
    for (int I = MI.getNumOperands() - 1; I >= 0; --I) {
      MachineOperand &MO = MI.getOperand(I);
      if (!MO.isReg() || !MO.isDef() || !MO.isImplicit() ||
          !MO.getReg().isPhysical())
        continue;
      MCPhysReg Reg = MO.getReg().asMCReg();
      if (declaresDef(MI, Reg, TRI) || !declaresDef(*Prev, Reg, TRI))
        continue;
      LLVM_DEBUG(dbgs() << "Z80FixupImplicitDefs: removing relocated def "
                        << printReg(Reg, &TRI) << " from: " << MI);
      MI.removeOperand(I);
      Changed = true;
    }
  }
  return Changed;
}

bool Z80FixupImplicitDefs::runOnMachineFunction(MachineFunction &MF) {
  const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    Changed |= removeRelocatedDefs(MBB, *TRI);
    for (MachineInstr &MI : MBB) {
      SmallSet<MCPhysReg, 4> SubRegDefs = getIntendedSubRegDefs(MI, *TRI);
      if (SubRegDefs.empty())
        continue;

      // Walk operands in reverse so removal doesn't invalidate indices.
      for (int I = MI.getNumOperands() - 1; I >= 0; --I) {
        MachineOperand &MO = MI.getOperand(I);
        if (!MO.isReg() || !MO.isDef() || !MO.isImplicit())
          continue;

        MCPhysReg Reg = MO.getReg().asMCReg();

        // Keep operands that are part of the intended defs.
        if (SubRegDefs.count(Reg))
          continue;

        // Check if this implicit-def is a super-register of any intended
        // sub-register def.  On Z80, this catches HL added by LiveVariables
        // when only L (or H) is defined.  Same for BC/DE/IX/IY pairs.
        bool IsSuperOfSubRegDef = false;
        for (MCPhysReg SD : SubRegDefs) {
          if (TRI->isSuperRegister(SD, Reg)) {
            IsSuperOfSubRegDef = true;
            break;
          }
        }

        if (!IsSuperOfSubRegDef)
          continue;

        LLVM_DEBUG(dbgs() << "Z80FixupImplicitDefs: removing implicit-def "
                          << printReg(Reg, TRI) << " from: " << MI);
        MI.removeOperand(I);
        Changed = true;
      }
    }
  }

  return Changed;
}

MachineFunctionPass *llvm::createZ80FixupImplicitDefsPass() {
  return new Z80FixupImplicitDefs;
}
