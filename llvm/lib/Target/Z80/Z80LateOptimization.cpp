//===-- Z80LateOptimization.cpp - Z80 Late Optimization -------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the Z80 late optimization pass.
//
// This pass performs IX-indexed store-to-load forwarding after pseudo
// instructions have been expanded. When a value is spilled to the stack via
// LD (IX+d),R and later reloaded via LD R',(IX+d), this pass replaces the
// reload with a direct LD R',R (or eliminates it if R'==R).
//
//===----------------------------------------------------------------------===//

#include "Z80LateOptimization.h"

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80.h"
#include "Z80Subtarget.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "z80-late-opt"

using namespace llvm;

// Custom DenseMapInfo for IX offsets.  The default DenseMapInfo<int8_t> uses
// -1 and -2 as sentinel values, which collide with valid IX offsets.
// Using int as the key type with out-of-range sentinels avoids this.
struct IXOffsetInfo {
  static inline int getEmptyKey() { return 256; }
  static inline int getTombstoneKey() { return 257; }
  static unsigned getHashValue(int V) {
    return DenseMapInfo<int>::getHashValue(V);
  }
  static bool isEqual(int LHS, int RHS) { return LHS == RHS; }
};

// Get the source register for an IX-indexed store instruction.
// Returns the physical register being stored, or Register() if not an
// IX-indexed store.
static Register getStoreIXdSrcReg(unsigned Opc) {
  switch (Opc) {
  case Z80::LD_IXd_A:
    return Z80::A;
  case Z80::LD_IXd_B:
    return Z80::B;
  case Z80::LD_IXd_C:
    return Z80::C;
  case Z80::LD_IXd_D:
    return Z80::D;
  case Z80::LD_IXd_E:
    return Z80::E;
  case Z80::LD_IXd_H:
    return Z80::H;
  case Z80::LD_IXd_L:
    return Z80::L;
  default:
    return Register();
  }
}

// Get the destination register for an IX-indexed load instruction.
// Returns the physical register being loaded, or Register() if not an
// IX-indexed load.
static Register getLoadIXdDstReg(unsigned Opc) {
  switch (Opc) {
  case Z80::LD_A_IXd:
    return Z80::A;
  case Z80::LD_B_IXd:
    return Z80::B;
  case Z80::LD_C_IXd:
    return Z80::C;
  case Z80::LD_D_IXd:
    return Z80::D;
  case Z80::LD_E_IXd:
    return Z80::E;
  case Z80::LD_H_IXd:
    return Z80::H;
  case Z80::LD_L_IXd:
    return Z80::L;
  default:
    return Register();
  }
}

// Get the LD r,r' opcode for two 8-bit physical registers.
// Returns 0 if no direct LD exists.
static unsigned getLD8Opcode(Register Dst, Register Src) {
  // Map register to table index
  auto regIdx = [](Register R) -> int {
    switch (R.id()) {
    case Z80::A:
      return 0;
    case Z80::B:
      return 1;
    case Z80::C:
      return 2;
    case Z80::D:
      return 3;
    case Z80::E:
      return 4;
    case Z80::H:
      return 5;
    case Z80::L:
      return 6;
    default:
      return -1;
    }
  };

  static const unsigned LDOpcodes[7][7] = {
      //       A            B            C            D            E H L
      /*A*/ {Z80::LD_A_A, Z80::LD_A_B, Z80::LD_A_C, Z80::LD_A_D, Z80::LD_A_E,
             Z80::LD_A_H, Z80::LD_A_L},
      /*B*/
      {Z80::LD_B_A, Z80::LD_B_B, Z80::LD_B_C, Z80::LD_B_D, Z80::LD_B_E,
       Z80::LD_B_H, Z80::LD_B_L},
      /*C*/
      {Z80::LD_C_A, Z80::LD_C_B, Z80::LD_C_C, Z80::LD_C_D, Z80::LD_C_E,
       Z80::LD_C_H, Z80::LD_C_L},
      /*D*/
      {Z80::LD_D_A, Z80::LD_D_B, Z80::LD_D_C, Z80::LD_D_D, Z80::LD_D_E,
       Z80::LD_D_H, Z80::LD_D_L},
      /*E*/
      {Z80::LD_E_A, Z80::LD_E_B, Z80::LD_E_C, Z80::LD_E_D, Z80::LD_E_E,
       Z80::LD_E_H, Z80::LD_E_L},
      /*H*/
      {Z80::LD_H_A, Z80::LD_H_B, Z80::LD_H_C, Z80::LD_H_D, Z80::LD_H_E,
       Z80::LD_H_H, Z80::LD_H_L},
      /*L*/
      {Z80::LD_L_A, Z80::LD_L_B, Z80::LD_L_C, Z80::LD_L_D, Z80::LD_L_E,
       Z80::LD_L_H, Z80::LD_L_L},
  };

  int di = regIdx(Dst), si = regIdx(Src);
  if (di < 0 || si < 0)
    return 0;
  return LDOpcodes[di][si];
}

// Invalidate all AvailValues entries where the stored register overlaps
// with the given clobbered register.
static void invalidateReg(DenseMap<int, MCPhysReg, IXOffsetInfo> &AvailValues,
                          const TargetRegisterInfo *TRI,
                          MCPhysReg ClobberedReg) {
  SmallVector<int, 4> ToErase;
  for (auto &KV : AvailValues) {
    if (TRI->regsOverlap(KV.second, ClobberedReg))
      ToErase.push_back(KV.first);
  }
  for (int K : ToErase)
    AvailValues.erase(K);
}

namespace {

class Z80LateOptimization : public MachineFunctionPass {
public:
  static char ID;

  Z80LateOptimization() : MachineFunctionPass(ID) {
    llvm::initializeZ80LateOptimizationPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
};

// Get the destination register for a LD r,(HL) instruction,
// or Register() if not one.
static Register getLoadHLindDstReg(unsigned Opc) {
  switch (Opc) {
  case Z80::LD_A_HLind:
    return Z80::A;
  case Z80::LD_B_HLind:
    return Z80::B;
  case Z80::LD_C_HLind:
    return Z80::C;
  case Z80::LD_D_HLind:
    return Z80::D;
  case Z80::LD_E_HLind:
    return Z80::E;
  default:
    return Register();
  }
}

// Get the source register for a LD (HL),r instruction,
// or Register() if not one.
static Register getStoreHLindSrcReg(unsigned Opc) {
  switch (Opc) {
  case Z80::LD_HLind_A:
    return Z80::A;
  case Z80::LD_HLind_B:
    return Z80::B;
  case Z80::LD_HLind_C:
    return Z80::C;
  case Z80::LD_HLind_D:
    return Z80::D;
  case Z80::LD_HLind_E:
    return Z80::E;
  default:
    return Register();
  }
}

// Get the source register for a LD A,r instruction, or Register() if not one.
static Register getLDArSrcReg(unsigned Opc) {
  switch (Opc) {
  case Z80::LD_A_B:
    return Z80::B;
  case Z80::LD_A_C:
    return Z80::C;
  case Z80::LD_A_D:
    return Z80::D;
  case Z80::LD_A_E:
    return Z80::E;
  case Z80::LD_A_H:
    return Z80::H;
  case Z80::LD_A_L:
    return Z80::L;
  default:
    return Register();
  }
}

// Get the LD r,A opcode for a given register r. Returns 0 if invalid.
static unsigned getLDrAOpcode(Register R) {
  switch (R.id()) {
  case Z80::B:
    return Z80::LD_B_A;
  case Z80::C:
    return Z80::LD_C_A;
  case Z80::D:
    return Z80::LD_D_A;
  case Z80::E:
    return Z80::LD_E_A;
  case Z80::H:
    return Z80::LD_H_A;
  case Z80::L:
    return Z80::LD_L_A;
  default:
    return 0;
  }
}

// Get the DEC r opcode for a given register r. Returns 0 if invalid.
static unsigned getDECrOpcode(Register R) {
  switch (R.id()) {
  case Z80::B:
    return Z80::DEC_B;
  case Z80::C:
    return Z80::DEC_C;
  case Z80::D:
    return Z80::DEC_D;
  case Z80::E:
    return Z80::DEC_E;
  case Z80::H:
    return Z80::DEC_H;
  case Z80::L:
    return Z80::DEC_L;
  default:
    return 0;
  }
}

// Get the LD r,#imm opcode for a given register r. Returns 0 if invalid.
static unsigned getLDrnOpcode(Register R) {
  switch (R.id()) {
  case Z80::A:
    return Z80::LD_A_n;
  case Z80::B:
    return Z80::LD_B_n;
  case Z80::C:
    return Z80::LD_C_n;
  case Z80::D:
    return Z80::LD_D_n;
  case Z80::E:
    return Z80::LD_E_n;
  case Z80::H:
    return Z80::LD_H_n;
  case Z80::L:
    return Z80::LD_L_n;
  default:
    return 0;
  }
}

// Check if a physical register is dead at a given point by scanning forward
// in the basic block. Returns true if the register is not used before being
// fully redefined or the end of the basic block.
//
// Tracks accumulated partial defs across instructions: e.g. for Reg=DE,
// seeing LD E,A followed by LD D,A (with no intervening use of DE/D/E)
// counts as a full redefinition.
static bool isRegDeadAfter(MachineBasicBlock::iterator After,
                           MachineBasicBlock &MBB,
                           const TargetRegisterInfo *TRI, MCPhysReg Reg) {
  // Collect sub-registers that must all be defined for Reg to be fully dead.
  // For leaf registers (e.g. E with no sub-regs), this stays empty and only
  // direct/super-register defs matter.
  SmallVector<MCPhysReg, 4> SubRegs;
  SmallVector<bool, 4> SubRegDefined;
  for (MCSubRegIterator SR(Reg, TRI); SR.isValid(); ++SR) {
    SubRegs.push_back(*SR);
    SubRegDefined.push_back(false);
  }

  for (auto I = After, E = MBB.end(); I != E; ++I) {
    bool HasUse = false, HasFullDef = false;
    for (const MachineOperand &MO : I->operands()) {
      if (!MO.isReg() || !MO.getReg().isPhysical())
        continue;
      if (!TRI->regsOverlap(MO.getReg(), Reg))
        continue;
      if (MO.readsReg())
        HasUse = true;
      if (MO.isDef()) {
        MCPhysReg DefReg = MO.getReg();
        // Direct or super-register def covers the entire register.
        if (DefReg == Reg || TRI->isSuperRegister(Reg, DefReg))
          HasFullDef = true;
        // Track partial defs: mark which sub-registers are covered.
        for (unsigned i = 0, e = SubRegs.size(); i != e; ++i) {
          if (!SubRegDefined[i] && (DefReg == SubRegs[i] ||
                                    TRI->isSuperRegister(SubRegs[i], DefReg)))
            SubRegDefined[i] = true;
        }
      }
    }
    if (HasUse)
      return false; // Used — register is live
    if (HasFullDef)
      return true; // Fully redefined without use — register is dead
    // Check if accumulated partial defs now cover all sub-registers.
    if (!SubRegs.empty() &&
        llvm::all_of(SubRegDefined, [](bool d) { return d; }))
      return true;
  }
  // End of basic block — check if register is live-in to any successor.
  // Use regsOverlap to catch sub/super-register relationships
  // (e.g. Reg=E but successor has DE as live-in).
  for (MachineBasicBlock *Succ : MBB.successors()) {
    for (const auto &LI : Succ->liveins()) {
      if (TRI->regsOverlap(LI.PhysReg, Reg))
        return false;
    }
  }
  return true;
}

bool Z80LateOptimization::runOnMachineFunction(MachineFunction &MF) {
  const auto &STI = MF.getSubtarget<Z80Subtarget>();
  const auto *TII = STI.getInstrInfo();
  const auto *TRI = STI.getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    // --- Peephole: POP rr; PUSH rr → (remove both) ---
    // When a register pair is popped and immediately pushed back, the stack
    // state is unchanged (SP net effect = 0, same value on stack). If the
    // register pair is dead after the push (overwritten before next use),
    // both instructions are redundant. Common on SM83 where consecutive
    // stack accesses via LDHL SP,# each need push/pop HL around them.
    for (MachineBasicBlock::iterator MII = MBB.begin(), MIE = MBB.end();
         MII != MIE;) {
      static const struct {
        unsigned PopOpc;
        unsigned PushOpc;
        MCPhysReg Reg;
      } PopPushPairs[] = {
          {Z80::POP_BC, Z80::PUSH_BC, Z80::BC},
          {Z80::POP_DE, Z80::PUSH_DE, Z80::DE},
          {Z80::POP_HL, Z80::PUSH_HL, Z80::HL},
      };

      unsigned Opc = MII->getOpcode();
      bool Matched = false;
      for (const auto &PP : PopPushPairs) {
        if (Opc != PP.PopOpc)
          continue;
        auto NextIt = std::next(MII);
        if (NextIt == MIE || NextIt->getOpcode() != PP.PushOpc)
          break;
        auto AfterPush = std::next(NextIt);
        if (!isRegDeadAfter(AfterPush, MBB, TRI, PP.Reg))
          break;
        LLVM_DEBUG(dbgs() << "  Removing redundant POP+PUSH: " << *MII);
        NextIt->eraseFromParent();
        MII = MBB.erase(MII);
        Changed = true;
        Matched = true;
        break;
      }
      if (!Matched)
        ++MII;
    }

    // --- Peephole: LD A,r; DEC A; LD r,A; OR A; JR NZ → DEC r; JR NZ ---
    // Replaces a 5-instruction decrement-and-branch sequence (28T, 6B) with
    // DEC r; JR NZ (14T, 3B). DEC r sets Z flag correctly for JR NZ, and
    // stays within the analyzable branch framework. Works on Z80 and SM83.
    for (MachineBasicBlock::iterator MII = MBB.begin(), MIE = MBB.end();
         MII != MIE;) {
      // Match: LD A,r (identify counter register r)
      Register CounterReg = getLDArSrcReg(MII->getOpcode());
      if (!CounterReg.isValid()) {
        ++MII;
        continue;
      }
      auto I1 = MII;
      auto I2 = std::next(I1);
      if (I2 == MIE) {
        ++MII;
        continue;
      }
      auto I3 = std::next(I2);
      if (I3 == MIE) {
        ++MII;
        continue;
      }
      auto I4 = std::next(I3);
      if (I4 == MIE) {
        ++MII;
        continue;
      }
      auto I5 = std::next(I4);
      if (I5 == MIE) {
        ++MII;
        continue;
      }

      // Match: DEC A; LD r,A; OR A; JR NZ,target
      if (I2->getOpcode() != Z80::DEC_A ||
          I3->getOpcode() != getLDrAOpcode(CounterReg) ||
          I4->getOpcode() != Z80::OR_A || I5->getOpcode() != Z80::JR_NZ_e) {
        ++MII;
        continue;
      }

      // The original sequence leaves A = r-1. The replacement doesn't
      // touch A, so we must verify A is dead after the sequence.
      if (!isRegDeadAfter(std::next(I5), MBB, TRI, Z80::A)) {
        ++MII;
        continue;
      }

      MachineBasicBlock *TargetMBB = I5->getOperand(0).getMBB();
      DebugLoc DL = I1->getDebugLoc();
      unsigned DECOpc = getDECrOpcode(CounterReg);
      LLVM_DEBUG(dbgs() << "  Loop counter peephole: LD A,"
                        << printReg(CounterReg, TRI) << " sequence → DEC "
                        << printReg(CounterReg, TRI) << "; JR NZ\n");
      I5->eraseFromParent();
      I4->eraseFromParent();
      I3->eraseFromParent();
      I2->eraseFromParent();
      MII = MBB.erase(I1);
      BuildMI(MBB, MII, DL, TII->get(DECOpc));
      BuildMI(MBB, MII, DL, TII->get(Z80::JR_NZ_e)).addMBB(TargetMBB);
      Changed = true;
    }

    // --- Peephole: XOR #0xFF → CPL ---
    // CPL (1 byte) is equivalent to XOR #0xFF (2 bytes) for the A register
    // value, but sets flags differently (CPL: H=1,N=1, others unchanged;
    // XOR: S,Z,P from result, H=1,N=0,C=0). Safe only when FLAGS is dead.
    for (MachineBasicBlock::iterator MII = MBB.begin(), MIE = MBB.end();
         MII != MIE;) {
      MachineInstr &MI = *MII;
      if (MI.getOpcode() == Z80::XOR_n && MI.getOperand(0).getImm() == 0xFF) {
        auto After = std::next(MII);
        if (isRegDeadAfter(After, MBB, TRI, Z80::FLAGS)) {
          LLVM_DEBUG(dbgs() << "  XOR #0xFF → CPL: " << MI);
          BuildMI(MBB, MI, MI.getDebugLoc(), TII->get(Z80::CPL));
          MII = MBB.erase(MII);
          Changed = true;
          continue;
        }
      }
      ++MII;
    }

    // --- Peephole: LD A,#0 → XOR A ---
    // XOR A (1 byte) sets A to 0 just like LD A,#0 (2 bytes), but also
    // sets FLAGS (Z=1, S=0, H=0, P=1, N=0, C=0). Safe when FLAGS is dead.
    for (MachineBasicBlock::iterator MII = MBB.begin(), MIE = MBB.end();
         MII != MIE;) {
      MachineInstr &MI = *MII;
      if (MI.getOpcode() == Z80::LD_A_n && MI.getOperand(0).getImm() == 0) {
        auto After = std::next(MII);
        if (isRegDeadAfter(After, MBB, TRI, Z80::FLAGS)) {
          LLVM_DEBUG(dbgs() << "  LD A,#0 → XOR A: " << MI);
          BuildMI(MBB, MI, MI.getDebugLoc(), TII->get(Z80::XOR_A));
          MII = MBB.erase(MII);
          Changed = true;
          continue;
        }
      }
      ++MII;
    }

    // --- Peephole: ALU #imm; ALU #imm → ALU #imm ---
    // When the same immediate ALU instruction appears consecutively, the
    // second is redundant for idempotent operations (AND, OR).
    // Most common case: AND #1; AND #1 after SBC A,A; AND #1 sequences.
    for (MachineBasicBlock::iterator MII = MBB.begin(), MIE = MBB.end();
         MII != MIE;) {
      MachineInstr &MI = *MII;
      auto NextIt = std::next(MII);
      if (NextIt != MIE && MI.getOpcode() == NextIt->getOpcode() &&
          (MI.getOpcode() == Z80::AND_n || MI.getOpcode() == Z80::OR_n) &&
          MI.getOperand(0).getImm() == NextIt->getOperand(0).getImm()) {
        LLVM_DEBUG(dbgs() << "  Removing redundant: " << *NextIt);
        NextIt->eraseFromParent();
        Changed = true;
        continue;
      }
      ++MII;
    }

    // --- Peephole: LD rr,#imm; LDHL SP,#; LD (HL),lo; INC HL; LD (HL),hi
    //             → LDHL SP,#; LD (HL),#lo; INC HL; LD (HL),#hi (SM83 only) ---
    // When a 16-bit constant is stored to the stack via a register pair,
    // replace with immediate stores to (HL). Saves 1 byte (8B → 7B) per
    // occurrence and frees the register pair.
    if (STI.hasSM83()) {
      for (MachineBasicBlock::iterator MII = MBB.begin(), MIE = MBB.end();
           MII != MIE;) {
        MachineInstr &MI = *MII;
        unsigned Opc = MI.getOpcode();

        bool IsBC = (Opc == Z80::LD_BC_nn);
        bool IsDE = (Opc == Z80::LD_DE_nn);
        if (!IsBC && !IsDE) {
          ++MII;
          continue;
        }
        if (!MI.getOperand(0).isImm()) {
          ++MII;
          continue;
        }

        // Match 5 consecutive instructions.
        auto I2 = std::next(MII);
        if (I2 == MIE || I2->getOpcode() != Z80::LDHL_SP_e) {
          ++MII;
          continue;
        }
        auto I3 = std::next(I2);
        if (I3 == MIE) {
          ++MII;
          continue;
        }
        unsigned ExpLo = IsBC ? Z80::LD_HLind_C : Z80::LD_HLind_E;
        if (I3->getOpcode() != ExpLo) {
          ++MII;
          continue;
        }

        auto I4 = std::next(I3);
        if (I4 == MIE || I4->getOpcode() != Z80::INC_HL) {
          ++MII;
          continue;
        }
        auto I5 = std::next(I4);
        if (I5 == MIE) {
          ++MII;
          continue;
        }
        unsigned ExpHi = IsBC ? Z80::LD_HLind_B : Z80::LD_HLind_D;
        if (I5->getOpcode() != ExpHi) {
          ++MII;
          continue;
        }

        // Register pair must be dead after the store sequence.
        MCPhysReg PairReg = IsBC ? Z80::BC : Z80::DE;
        if (!isRegDeadAfter(std::next(I5), MBB, TRI, PairReg)) {
          ++MII;
          continue;
        }

        int64_t Imm = MI.getOperand(0).getImm();
        LLVM_DEBUG(dbgs() << "  Folding 16-bit const store: " << MI);

        // Replace LD (HL),lo → LD (HL),#imm_lo
        BuildMI(MBB, *I3, I3->getDebugLoc(), TII->get(Z80::LD_HLind_n))
            .addImm(Imm & 0xFF);
        I3->eraseFromParent();

        // Replace LD (HL),hi → LD (HL),#imm_hi
        BuildMI(MBB, *I5, I5->getDebugLoc(), TII->get(Z80::LD_HLind_n))
            .addImm((Imm >> 8) & 0xFF);
        I5->eraseFromParent();

        // Remove LD rr,#imm
        MII = MBB.erase(MII);
        Changed = true;
      }
    }

    // --- Peephole: consecutive LDHL SP,#N → INC/DEC HL (SM83 only) ---
    // When two LDHL SP,# instructions target adjacent offsets with only
    // non-HL-modifying instructions between them, replace the second LDHL
    // with INC HL or DEC HL. Saves 1 byte (2B → 1B) per occurrence.
    // Common in consecutive byte-at-a-time stack initialization.
    if (STI.hasSM83()) {
      for (MachineBasicBlock::iterator MII = MBB.begin(), MIE = MBB.end();
           MII != MIE; ++MII) {
        if (MII->getOpcode() != Z80::LDHL_SP_e)
          continue;
        if (!MII->getOperand(0).isImm())
          continue;
        int64_t Offset1 = MII->getOperand(0).getImm();

        // Scan forward to find the next LDHL SP,#. Bail if any
        // intervening instruction modifies HL, modifies SP, or has
        // unmodeled side effects. SP changes must be caught explicitly
        // because PUSH/POP don't declare SP in their Defs.
        auto It = std::next(MII);
        bool Clobbered = false;
        while (It != MIE && It->getOpcode() != Z80::LDHL_SP_e) {
          // PUSH/POP modify SP but don't declare it as Def.
          if (It->isCall() || It->isReturn() || It->hasUnmodeledSideEffects() ||
              It->getOpcode() == Z80::PUSH_BC ||
              It->getOpcode() == Z80::PUSH_DE ||
              It->getOpcode() == Z80::PUSH_HL ||
              It->getOpcode() == Z80::PUSH_AF ||
              It->getOpcode() == Z80::POP_BC ||
              It->getOpcode() == Z80::POP_DE ||
              It->getOpcode() == Z80::POP_AF ||
              It->getOpcode() == Z80::ADD_SP_e) {
            // POP_HL also modifies HL, but we catch it via Defs below.
            Clobbered = true;
            break;
          }
          // Check explicit and implicit defs for HL and SP.
          for (const MachineOperand &MO : It->operands()) {
            if (MO.isReg() && MO.isDef() && MO.getReg().isPhysical() &&
                (TRI->regsOverlap(MO.getReg(), Z80::HL) ||
                 TRI->regsOverlap(MO.getReg(), Z80::SP))) {
              Clobbered = true;
              break;
            }
          }
          if (Clobbered)
            break;
          for (MCPhysReg Def : TII->get(It->getOpcode()).implicit_defs()) {
            if (TRI->regsOverlap(Def, Z80::HL) ||
                TRI->regsOverlap(Def, Z80::SP)) {
              Clobbered = true;
              break;
            }
          }
          if (Clobbered)
            break;
          ++It;
        }
        if (Clobbered || It == MIE)
          continue;
        if (It->getOpcode() != Z80::LDHL_SP_e || !It->getOperand(0).isImm())
          continue;

        int64_t Offset2 = It->getOperand(0).getImm();
        int64_t Diff = Offset2 - Offset1;
        if (Diff != 1 && Diff != -1)
          continue;
        // LDHL sets FLAGS (H,C), INC/DEC HL does not. Verify FLAGS is dead.
        if (!isRegDeadAfter(std::next(It), MBB, TRI, Z80::FLAGS))
          continue;

        unsigned NewOpc = (Diff == 1) ? Z80::INC_HL : Z80::DEC_HL;
        LLVM_DEBUG(dbgs() << "  LDHL SP,#" << Offset2 << " → "
                          << (Diff == 1 ? "INC" : "DEC") << " HL\n");
        BuildMI(MBB, *It, It->getDebugLoc(), TII->get(NewOpc));
        It->eraseFromParent();
        Changed = true;
      }
    }

    // --- Peephole: fold constant into XOR compare (CMP_Z16 + imm) ---
    // When a XOR-based 16-bit compare uses a constant loaded into a register
    // pair, fold the constant into XOR immediate instructions.
    // LD rr,#imm; LD A,X; XOR rhi; LD B,A; LD A,Y; XOR rlo; OR B
    // → LD A,X; XOR #hi; LD B,A; LD A,Y; XOR #lo; OR B
    // Saves 1 byte (9B → 8B) per occurrence and frees the register pair.
    // Applies to both Z80 (XOR_CMP_Z16 for i32/i64) and SM83 (SM83_CMP_Z16).
    {
      for (MachineBasicBlock::iterator MII = MBB.begin(), MIE = MBB.end();
           MII != MIE;) {
        MachineInstr &MI = *MII;
        unsigned Opc = MI.getOpcode();

        bool IsBC = (Opc == Z80::LD_BC_nn);
        bool IsDE = (Opc == Z80::LD_DE_nn);
        if (!IsBC && !IsDE) {
          ++MII;
          continue;
        }
        if (!MI.getOperand(0).isImm()) {
          ++MII;
          continue;
        }

        // Match 7 consecutive instructions.
        auto I2 = std::next(MII);
        if (I2 == MIE) {
          ++MII;
          continue;
        }
        // I2: LD A,X (load high byte of compared value)
        Register I2Src = getLDArSrcReg(I2->getOpcode());
        if (!I2Src.isValid() && I2->getOpcode() != Z80::LD_A_HLind) {
          ++MII;
          continue;
        }

        auto I3 = std::next(I2);
        if (I3 == MIE) {
          ++MII;
          continue;
        }
        unsigned ExpXorHi = IsBC ? Z80::XOR_B : Z80::XOR_D;
        if (I3->getOpcode() != ExpXorHi) {
          ++MII;
          continue;
        }

        auto I4 = std::next(I3);
        if (I4 == MIE || I4->getOpcode() != Z80::LD_B_A) {
          ++MII;
          continue;
        }

        auto I5 = std::next(I4);
        if (I5 == MIE) {
          ++MII;
          continue;
        }
        // I5: LD A,Y (load low byte of compared value)
        Register I5Src = getLDArSrcReg(I5->getOpcode());
        if (!I5Src.isValid() && I5->getOpcode() != Z80::LD_A_HLind) {
          ++MII;
          continue;
        }

        auto I6 = std::next(I5);
        if (I6 == MIE) {
          ++MII;
          continue;
        }
        unsigned ExpXorLo = IsBC ? Z80::XOR_C : Z80::XOR_E;
        if (I6->getOpcode() != ExpXorLo) {
          ++MII;
          continue;
        }

        auto I7 = std::next(I6);
        if (I7 == MIE || I7->getOpcode() != Z80::OR_B) {
          ++MII;
          continue;
        }

        // Ensure lhs registers don't overlap with the constant pair.
        MCPhysReg PairReg = IsBC ? Z80::BC : Z80::DE;
        if (I2Src.isValid() && TRI->regsOverlap(I2Src, PairReg)) {
          ++MII;
          continue;
        }
        if (I5Src.isValid() && TRI->regsOverlap(I5Src, PairReg)) {
          ++MII;
          continue;
        }

        // The constant pair must be dead after OR B.
        // For BC: B is overwritten by LD B,A (I4) with the XOR result (same
        // value in both original and folded code), so only C matters.
        // For DE: neither D nor E is overwritten, so both must be dead.
        if (IsBC) {
          if (!isRegDeadAfter(std::next(I7), MBB, TRI, Z80::C)) {
            ++MII;
            continue;
          }
        } else {
          if (!isRegDeadAfter(std::next(I7), MBB, TRI, Z80::DE)) {
            ++MII;
            continue;
          }
        }

        int64_t Imm = MI.getOperand(0).getImm();
        int64_t HiByte = (Imm >> 8) & 0xFF;
        int64_t LoByte = Imm & 0xFF;
        LLVM_DEBUG(dbgs() << "  Folding CMP_Z16 constant: " << MI);

        // Handle XOR rhi: replace with XOR #hi, or remove if hi == 0.
        if (HiByte != 0) {
          BuildMI(MBB, *I3, I3->getDebugLoc(), TII->get(Z80::XOR_n))
              .addImm(HiByte);
        } else {
          // XOR #0 is identity. Also fold LD A,X; LD B,A → LD B,X.
          // I2 is LD A,X, I4 is LD B,A. With XOR removed, this is LD B,X.
          unsigned LdBOpc = 0;
          if (I2Src.isValid())
            LdBOpc = getLD8Opcode(Z80::B, I2Src);
          else if (I2->getOpcode() == Z80::LD_A_HLind)
            LdBOpc = Z80::LD_B_HLind;
          if (LdBOpc) {
            // Skip LD B,B (self-move NOP when I2Src == B).
            if (!(I2Src.isValid() && I2Src == Z80::B))
              BuildMI(MBB, *I2, I2->getDebugLoc(), TII->get(LdBOpc));
            I2->eraseFromParent();
            I4->eraseFromParent();
          }
        }
        I3->eraseFromParent();

        // Handle XOR rlo: replace with XOR #lo, or remove if lo == 0.
        if (LoByte != 0) {
          BuildMI(MBB, *I6, I6->getDebugLoc(), TII->get(Z80::XOR_n))
              .addImm(LoByte);
        }
        I6->eraseFromParent();

        // Remove LD rr,#imm
        MII = MBB.erase(MII);
        Changed = true;
      }
    }

    // --- Peephole: LD A,(HL); INC/DEC HL → LD A,(HL+)/(HL-) (SM83 only) ---
    // SM83 has post-increment/decrement LD instructions that combine a load
    // or store with an HL adjustment in a single byte.
    // Patterns:
    //   LD A,(HL); INC HL → LD A,(HL+)   (2B → 1B)
    //   LD (HL),A; INC HL → LD (HL+),A   (2B → 1B)
    //   LD A,(HL); DEC HL → LD A,(HL-)   (2B → 1B)
    //   LD (HL),A; DEC HL → LD (HL-),A   (2B → 1B)
    //
    // Extended: when r != A and A is dead after the sequence:
    //   LD r,(HL); INC HL → LD A,(HL+); LD r,A   (2B → 2B, saves 4T)
    //   LD (HL),r; INC HL → LD A,r; LD (HL+),A   (2B → 2B, saves 4T)
    //   (same for DEC HL variants with HL-/HL-)
    if (STI.hasSM83()) {
      for (MachineBasicBlock::iterator MII = MBB.begin(), MIE = MBB.end();
           MII != MIE;) {
        MachineInstr &MI = *MII;
        auto NextIt = std::next(MII);
        if (NextIt == MIE) {
          ++MII;
          continue;
        }

        unsigned Opc = MI.getOpcode();
        unsigned NextOpc = NextIt->getOpcode();

        // Direct r=A patterns: 2B → 1B (size + speed win)
        unsigned NewOpc = 0;
        if (NextOpc == Z80::INC_HL) {
          if (Opc == Z80::LD_A_HLind)
            NewOpc = Z80::LD_A_HLI;
          else if (Opc == Z80::LD_HLind_A)
            NewOpc = Z80::LD_HLI_A;
        } else if (NextOpc == Z80::DEC_HL) {
          if (Opc == Z80::LD_A_HLind)
            NewOpc = Z80::LD_A_HLD;
          else if (Opc == Z80::LD_HLind_A)
            NewOpc = Z80::LD_HLD_A;
        }

        if (NewOpc) {
          LLVM_DEBUG(dbgs() << "  LD+INC/DEC HL → LD (HL+/-): " << MI);
          BuildMI(MBB, MI, MI.getDebugLoc(), TII->get(NewOpc));
          NextIt->eraseFromParent();
          MII = MBB.erase(MII);
          Changed = true;
          continue;
        }

        // Extended r!=A patterns: 2B → 2B (speed win only, saves 4T)
        // LD r,(HL); INC/DEC HL → LD A,(HL+/-); LD r,A  (requires A dead)
        // LD (HL),r; INC/DEC HL → LD A,r; LD (HL+/-),A  (requires A dead)
        if (NextOpc == Z80::INC_HL || NextOpc == Z80::DEC_HL) {
          bool IsInc = (NextOpc == Z80::INC_HL);

          Register LoadDst = getLoadHLindDstReg(Opc);
          Register StoreSrc = getStoreHLindSrcReg(Opc);
          // Exclude A: LD A,(HL) → LD A,(HL+) is handled directly,
          // and LD (HL),A → LD A,A; LD (HL+),A produces a useless LD A,A.
          if (LoadDst == Z80::A)
            LoadDst = Register();
          if (StoreSrc == Z80::A)
            StoreSrc = Register();

          // Skip if this load is part of a 16-bit HL load pattern that the
          // later peephole will fold more profitably (5B → 3B vs our 2B → 2B).
          // Pattern: LD C/E,(HL); INC HL; LD B/D,(HL); LD L,C/E; LD H,B/D
          if (LoadDst.isValid() && IsInc) {
            auto I3 = std::next(NextIt);
            if (I3 != MIE) {
              unsigned HiOpc = (LoadDst == Z80::C)   ? Z80::LD_B_HLind
                               : (LoadDst == Z80::E) ? Z80::LD_D_HLind
                                                     : 0;
              if (HiOpc && I3->getOpcode() == HiOpc) {
                ++MII;
                continue; // Let 16-bit HL load peephole handle it
              }
            }
          }

          if (LoadDst.isValid() || StoreSrc.isValid()) {
            auto AfterSeq = std::next(NextIt);
            if (isRegDeadAfter(AfterSeq, MBB, TRI, Z80::A)) {
              DebugLoc DL = MI.getDebugLoc();
              unsigned HLOpc = IsInc ? Z80::LD_A_HLI : Z80::LD_A_HLD;
              unsigned HLSOpc = IsInc ? Z80::LD_HLI_A : Z80::LD_HLD_A;

              if (LoadDst.isValid()) {
                // LD r,(HL); INC/DEC HL → LD A,(HL+/-); LD r,A
                LLVM_DEBUG(dbgs() << "  LD r,(HL)+INC/DEC → HL+/-: " << MI);
                BuildMI(MBB, MI, DL, TII->get(HLOpc));
                BuildMI(MBB, MI, DL, TII->get(getLDrAOpcode(LoadDst)));
              } else {
                // LD (HL),r; INC/DEC HL → LD A,r; LD (HL+/-),A
                LLVM_DEBUG(dbgs() << "  LD (HL),r+INC/DEC → HL+/-: " << MI);
                BuildMI(MBB, MI, DL, TII->get(getLD8Opcode(Z80::A, StoreSrc)));
                BuildMI(MBB, MI, DL, TII->get(HLSOpc));
              }

              NextIt->eraseFromParent();
              MII = MBB.erase(MII);
              Changed = true;
              continue;
            }
          }
        }

        ++MII;
      }

      // --- Peephole: 16-bit HL load via HL+ (SM83 only) ---
      // When loading a 16-bit value from (HL) into HL itself via BC or DE:
      //   LD lo,(HL); INC HL; LD hi,(HL); LD L,lo; LD H,hi  (5B)
      // → LD A,(HL+); LD H,(HL); LD L,A                     (3B, saves 2B)
      // LD A,(HL+) loads lo byte and increments HL in one instruction.
      // LD H,(HL) reads the hi byte (HL still points to hi) before writing H.
      // LD L,A completes the 16-bit value in HL.
      // Conditions: A dead after (clobbered), register pair dead after (not
      // loaded).
      for (MachineBasicBlock::iterator MII = MBB.begin(), MIE = MBB.end();
           MII != MIE;) {
        MachineInstr &MI = *MII;
        unsigned Opc = MI.getOpcode();

        // I1: LD C,(HL) or LD E,(HL)
        bool IsBC = (Opc == Z80::LD_C_HLind);
        bool IsDE = (Opc == Z80::LD_E_HLind);
        if (!IsBC && !IsDE) {
          ++MII;
          continue;
        }

        auto I2 = std::next(MII);
        if (I2 == MIE || I2->getOpcode() != Z80::INC_HL) {
          ++MII;
          continue;
        }
        auto I3 = std::next(I2);
        if (I3 == MIE) {
          ++MII;
          continue;
        }
        unsigned ExpHi = IsBC ? Z80::LD_B_HLind : Z80::LD_D_HLind;
        if (I3->getOpcode() != ExpHi) {
          ++MII;
          continue;
        }

        auto I4 = std::next(I3);
        if (I4 == MIE) {
          ++MII;
          continue;
        }
        unsigned ExpLdL = IsBC ? Z80::LD_L_C : Z80::LD_L_E;
        if (I4->getOpcode() != ExpLdL) {
          ++MII;
          continue;
        }

        auto I5 = std::next(I4);
        if (I5 == MIE) {
          ++MII;
          continue;
        }
        unsigned ExpLdH = IsBC ? Z80::LD_H_B : Z80::LD_H_D;
        if (I5->getOpcode() != ExpLdH) {
          ++MII;
          continue;
        }

        auto After = std::next(I5);
        if (!isRegDeadAfter(After, MBB, TRI, Z80::A)) {
          ++MII;
          continue;
        }
        MCPhysReg PairReg = IsBC ? Z80::BC : Z80::DE;
        if (!isRegDeadAfter(After, MBB, TRI, PairReg)) {
          ++MII;
          continue;
        }

        LLVM_DEBUG(dbgs() << "  16-bit HL load via HL+: " << MI);
        DebugLoc DL = MI.getDebugLoc();
        BuildMI(MBB, MI, DL, TII->get(Z80::LD_A_HLI));
        BuildMI(MBB, MI, DL, TII->get(Z80::LD_H_HLind));
        BuildMI(MBB, MI, DL, TII->get(Z80::LD_L_A));

        I5->eraseFromParent();
        I4->eraseFromParent();
        I3->eraseFromParent();
        I2->eraseFromParent();
        MII = MBB.erase(MII);
        Changed = true;
      }
    }

    // --- SM83 SP-relative store-to-load forwarding ---
    // On SM83, stack access uses LDHL SP,#N; LD (HL),r / LD (HL),#imm.
    // Track what values (register or immediate) are at each stack offset,
    // then forward to subsequent loads to eliminate redundant LDHL sequences.
    if (MF.getSubtarget<Z80Subtarget>().hasSM83()) {
      // Each slot can hold either a register value or an immediate.
      struct SlotVal {
        bool IsImm = false;
        MCPhysReg Reg = 0;
        uint8_t Imm = 0;
      };
      DenseMap<int, SlotVal, IXOffsetInfo> SPSlots;
      int SPDelta = 0;

      auto invalidateSlotReg = [&](const TargetRegisterInfo *TRI,
                                   MCPhysReg Reg) {
        SmallVector<int, 4> ToErase;
        for (auto &KV : SPSlots) {
          if (!KV.second.IsImm && TRI->regsOverlap(KV.second.Reg, Reg))
            ToErase.push_back(KV.first);
        }
        for (int K : ToErase)
          SPSlots.erase(K);
      };

      for (MachineBasicBlock::iterator MII = MBB.begin(), MIE = MBB.end();
           MII != MIE;) {
        MachineInstr &MI = *MII;
        unsigned Opc = MI.getOpcode();

        // Track SP changes.
        if (Opc == Z80::PUSH_AF || Opc == Z80::PUSH_BC || Opc == Z80::PUSH_DE ||
            Opc == Z80::PUSH_HL) {
          SPDelta -= 2;
          // PUSH writes to SPDelta+0 and SPDelta+1, invalidate those slots.
          SPSlots.erase(SPDelta);
          SPSlots.erase(SPDelta + 1);
          ++MII;
          continue;
        }
        if (Opc == Z80::POP_AF || Opc == Z80::POP_BC || Opc == Z80::POP_DE ||
            Opc == Z80::POP_HL) {
          // Invalidate slots at the popped location (no longer on stack).
          SPSlots.erase(SPDelta);
          SPSlots.erase(SPDelta + 1);
          SPDelta += 2;
          for (const MachineOperand &MO : MI.operands()) {
            if (MO.isReg() && MO.isDef() && MO.getReg().isPhysical())
              invalidateSlotReg(TRI, MO.getReg());
          }
          ++MII;
          continue;
        }
        if (Opc == Z80::ADD_SP_e) {
          int8_t Adj = (int8_t)(MI.getOperand(0).getImm() & 0xFF);
          SPDelta += Adj;
          ++MII;
          continue;
        }

        // Match LDHL SP,#N followed by store or load pattern.
        // (Check before side-effects: LDHL inherits hasSideEffects=1
        // from the conservative Z80Inst base but is actually safe.)
        if (Opc == Z80::LDHL_SP_e) {
          int8_t Imm = (int8_t)(MI.getOperand(0).getImm() & 0xFF);
          int AbsOff = SPDelta + Imm;

          auto It1 = std::next(MII);
          if (It1 == MIE) {
            ++MII;
            continue;
          }

          // Helper: check if a SlotVal matches a new store value.
          auto slotMatches = [](const SlotVal &Slot, bool NewIsImm,
                                MCPhysReg NewReg, uint8_t NewImm) -> bool {
            if (Slot.IsImm != NewIsImm)
              return false;
            if (Slot.IsImm)
              return Slot.Imm == NewImm;
            return Slot.Reg == NewReg;
          };

          // Helper: try to eliminate a redundant 16-bit store sequence.
          // Returns true if eliminated (LDHL + 3 instructions erased).
          auto tryElimRedundantStore =
              [&](int AbsOff, bool LoIsImm, MCPhysReg LoReg, uint8_t LoImm,
                  bool HiIsImm, MCPhysReg HiReg, uint8_t HiImm,
                  MachineBasicBlock::iterator LDHL,
                  MachineBasicBlock::iterator S1,
                  MachineBasicBlock::iterator Mid,
                  MachineBasicBlock::iterator S2) -> bool {
            auto AvLo = SPSlots.find(AbsOff);
            auto AvHi = SPSlots.find(AbsOff + 1);
            if (AvLo == SPSlots.end() || AvHi == SPSlots.end())
              return false;
            if (!slotMatches(AvLo->second, LoIsImm, LoReg, LoImm) ||
                !slotMatches(AvHi->second, HiIsImm, HiReg, HiImm))
              return false;
            // Values match. Safe to remove if HL and FLAGS are dead after.
            auto AfterStore = std::next(S2);
            if (!isRegDeadAfter(AfterStore, MBB, TRI, Z80::HL) ||
                !isRegDeadAfter(AfterStore, MBB, TRI, Z80::FLAGS))
              return false;
            LLVM_DEBUG(dbgs() << "  SM83 eliminating redundant store SP+"
                              << AbsOff << "\n");
            S2->eraseFromParent();
            Mid->eraseFromParent();
            S1->eraseFromParent();
            MII = MBB.erase(LDHL);
            Changed = true;
            return true;
          };

          // --- 16-bit immediate store: LDHL; LD (HL),#lo; INC HL; LD (HL),#hi
          if (It1->getOpcode() == Z80::LD_HLind_n) {
            auto It2 = std::next(It1);
            if (It2 != MIE && It2->getOpcode() == Z80::INC_HL) {
              auto It3 = std::next(It2);
              if (It3 != MIE && It3->getOpcode() == Z80::LD_HLind_n) {
                uint8_t LoVal = (uint8_t)(It1->getOperand(0).getImm() & 0xFF);
                uint8_t HiVal = (uint8_t)(It3->getOperand(0).getImm() & 0xFF);
                // Try redundant store elimination.
                if (tryElimRedundantStore(AbsOff, true, 0, LoVal, true, 0,
                                          HiVal, MII, It1, It2, It3))
                  continue;
                SlotVal SLo, SHi;
                SLo.IsImm = true;
                SLo.Imm = LoVal;
                SHi.IsImm = true;
                SHi.Imm = HiVal;
                SPSlots[AbsOff] = SLo;
                SPSlots[AbsOff + 1] = SHi;
                LLVM_DEBUG(dbgs() << "  SM83 imm store SP+" << AbsOff << " <- #"
                                  << (int)SLo.Imm << ", SP+" << (AbsOff + 1)
                                  << " <- #" << (int)SHi.Imm << "\n");
                MII = std::next(It3);
                continue;
              }
            }
            // 8-bit immediate store
            SlotVal S;
            S.IsImm = true;
            S.Imm = (uint8_t)(It1->getOperand(0).getImm() & 0xFF);
            SPSlots[AbsOff] = S;
            MII = std::next(It1);
            continue;
          }

          // --- 16-bit register store: LDHL; LD (HL),rlo; INC HL; LD (HL),rhi
          Register StoreSrc1 = getStoreHLindSrcReg(It1->getOpcode());
          if (StoreSrc1.isValid()) {
            auto It2 = std::next(It1);
            if (It2 != MIE && It2->getOpcode() == Z80::INC_HL) {
              auto It3 = std::next(It2);
              if (It3 != MIE) {
                Register StoreSrc2 = getStoreHLindSrcReg(It3->getOpcode());
                if (StoreSrc2.isValid()) {
                  // Try redundant store elimination.
                  if (tryElimRedundantStore(AbsOff, false, StoreSrc1, 0, false,
                                            StoreSrc2, 0, MII, It1, It2, It3))
                    continue;
                  SlotVal SLo, SHi;
                  SLo.Reg = StoreSrc1;
                  SHi.Reg = StoreSrc2;
                  SPSlots[AbsOff] = SLo;
                  SPSlots[AbsOff + 1] = SHi;
                  LLVM_DEBUG(dbgs() << "  SM83 reg store SP+" << AbsOff
                                    << " <- " << printReg(StoreSrc1, TRI)
                                    << ", SP+" << (AbsOff + 1) << " <- "
                                    << printReg(StoreSrc2, TRI) << "\n");
                  MII = std::next(It3);
                  continue;
                }
              }
            }
            // 8-bit register store
            SlotVal S;
            S.Reg = StoreSrc1;
            SPSlots[AbsOff] = S;
            MII = std::next(It1);
            continue;
          }

          // --- HL+ register store: LDHL; LD A,r; LD (HL+),A; LD (HL),r2
          {
            Register SrcLo = getLDArSrcReg(It1->getOpcode());
            // Only B/C/D/E — H/L can't be source (LDHL clobbered HL).
            if (SrcLo.isValid() && SrcLo != Z80::H && SrcLo != Z80::L) {
              auto It2 = std::next(It1);
              if (It2 != MIE && It2->getOpcode() == Z80::LD_HLI_A) {
                auto It3 = std::next(It2);
                if (It3 != MIE) {
                  Register StoreSrc2 = getStoreHLindSrcReg(It3->getOpcode());
                  if (StoreSrc2.isValid()) {
                    // Try redundant store elimination.
                    if (tryElimRedundantStore(AbsOff, false, SrcLo, 0, false,
                                              StoreSrc2, 0, MII, It1, It2, It3))
                      continue;
                    SlotVal SLo, SHi;
                    SLo.Reg = SrcLo;
                    SHi.Reg = StoreSrc2;
                    SPSlots[AbsOff] = SLo;
                    SPSlots[AbsOff + 1] = SHi;
                    LLVM_DEBUG(dbgs() << "  SM83 HL+ store SP+" << AbsOff
                                      << " <- " << printReg(SrcLo, TRI)
                                      << ", SP+" << (AbsOff + 1) << " <- "
                                      << printReg(StoreSrc2, TRI) << "\n");
                    MII = std::next(It3);
                    continue;
                  }
                }
              }
            }
          }

          // --- 16-bit load: LDHL; LD lo,(HL); INC HL; LD hi,(HL)
          Register LoadDst1 = getLoadHLindDstReg(It1->getOpcode());
          if (LoadDst1.isValid()) {
            auto It2 = std::next(It1);
            if (It2 != MIE && It2->getOpcode() == Z80::INC_HL) {
              auto It3 = std::next(It2);
              if (It3 != MIE) {
                Register LoadDst2 = getLoadHLindDstReg(It3->getOpcode());
                if (LoadDst2.isValid()) {
                  auto AvLo = SPSlots.find(AbsOff);
                  auto AvHi = SPSlots.find(AbsOff + 1);
                  if (AvLo != SPSlots.end() && AvHi != SPSlots.end()) {
                    SlotVal &SLo = AvLo->second;
                    SlotVal &SHi = AvHi->second;
                    // Forwarding removes LDHL which sets HL and FLAGS.
                    // Verify both are dead after the load sequence.
                    auto AfterLoad = std::next(It3);
                    if (!isRegDeadAfter(AfterLoad, MBB, TRI, Z80::HL) ||
                        !isRegDeadAfter(AfterLoad, MBB, TRI, Z80::FLAGS)) {
                      // Can't forward — fall through to tracking update.
                      invalidateSlotReg(TRI, LoadDst1);
                      invalidateSlotReg(TRI, LoadDst2);
                      SPSlots[AbsOff] = {false, MCPhysReg(LoadDst1), 0};
                      SPSlots[AbsOff + 1] = {false, MCPhysReg(LoadDst2), 0};
                      MII = std::next(It3);
                      continue;
                    }
                    // Build replacement instructions.
                    DebugLoc DL = MI.getDebugLoc();
                    bool CanForward = true;
                    // For register sources: can't use H/L (LDHL clobbers).
                    if (!SLo.IsImm && (SLo.Reg == Z80::H || SLo.Reg == Z80::L))
                      CanForward = false;
                    if (!SHi.IsImm && (SHi.Reg == Z80::H || SHi.Reg == Z80::L))
                      CanForward = false;
                    // Check reg-reg copy feasibility and ordering.
                    if (CanForward && !SLo.IsImm && !SHi.IsImm) {
                      // Both register: check for circular dependency.
                      bool LoIsNop = (LoadDst1 == SLo.Reg);
                      bool HiIsNop = (LoadDst2 == SHi.Reg);
                      unsigned OpcLo =
                          LoIsNop ? 0 : getLD8Opcode(LoadDst1, SLo.Reg);
                      unsigned OpcHi =
                          HiIsNop ? 0 : getLD8Opcode(LoadDst2, SHi.Reg);
                      if (!LoIsNop && !OpcLo)
                        CanForward = false;
                      if (!HiIsNop && !OpcHi)
                        CanForward = false;
                      if (CanForward) {
                        bool HiFirst = TRI->regsOverlap(LoadDst1, SHi.Reg);
                        if (HiFirst && TRI->regsOverlap(LoadDst2, SLo.Reg))
                          CanForward = false; // Circular.
                        if (CanForward) {
                          LLVM_DEBUG(dbgs() << "  SM83 fwd 16-bit reg SP+"
                                            << AbsOff << "\n");
                          if (HiFirst) {
                            if (OpcHi)
                              BuildMI(MBB, MI, DL, TII->get(OpcHi));
                            if (OpcLo)
                              BuildMI(MBB, MI, DL, TII->get(OpcLo));
                          } else {
                            if (OpcLo)
                              BuildMI(MBB, MI, DL, TII->get(OpcLo));
                            if (OpcHi)
                              BuildMI(MBB, MI, DL, TII->get(OpcHi));
                          }
                          It3->eraseFromParent();
                          It2->eraseFromParent();
                          It1->eraseFromParent();
                          MII = MBB.erase(MII);
                          Changed = true;
                          invalidateSlotReg(TRI, LoadDst1);
                          invalidateSlotReg(TRI, LoadDst2);
                          SPSlots[AbsOff] = {false, MCPhysReg(LoadDst1), 0};
                          SPSlots[AbsOff + 1] = {false, MCPhysReg(LoadDst2), 0};
                          continue;
                        }
                      }
                    }
                    // At least one immediate: generate LD r,#imm for imm
                    // slots and LD r,src for register slots.
                    if (CanForward) {
                      // Pre-validate all opcodes before emitting anything,
                      // to avoid partially-emitted instructions on failure.
                      auto getSlotOpc = [&](Register Dst,
                                            SlotVal &S) -> unsigned {
                        if (S.IsImm)
                          return getLDrnOpcode(Dst);
                        return (Dst == S.Reg) ? ~0u : getLD8Opcode(Dst, S.Reg);
                      };
                      unsigned OpcLo = getSlotOpc(LoadDst1, SLo);
                      unsigned OpcHi = getSlotOpc(LoadDst2, SHi);
                      if (!OpcLo || !OpcHi)
                        CanForward = false;
                    }
                    if (CanForward) {
                      bool HiFirst = false;
                      if (!SHi.IsImm && TRI->regsOverlap(LoadDst1, SHi.Reg))
                        HiFirst = true;

                      LLVM_DEBUG(dbgs() << "  SM83 fwd 16-bit imm/reg SP+"
                                        << AbsOff << "\n");
                      auto emitLoad = [&](Register Dst, SlotVal &S) {
                        if (S.IsImm) {
                          BuildMI(MBB, MI, DL, TII->get(getLDrnOpcode(Dst)))
                              .addImm(S.Imm);
                        } else if (Dst != S.Reg) {
                          BuildMI(MBB, MI, DL,
                                  TII->get(getLD8Opcode(Dst, S.Reg)));
                        }
                      };
                      if (HiFirst) {
                        emitLoad(LoadDst2, SHi);
                        emitLoad(LoadDst1, SLo);
                      } else {
                        emitLoad(LoadDst1, SLo);
                        emitLoad(LoadDst2, SHi);
                      }
                      It3->eraseFromParent();
                      It2->eraseFromParent();
                      It1->eraseFromParent();
                      MII = MBB.erase(MII);
                      Changed = true;
                      invalidateSlotReg(TRI, LoadDst1);
                      invalidateSlotReg(TRI, LoadDst2);
                      SPSlots[AbsOff] = {false, MCPhysReg(LoadDst1), 0};
                      SPSlots[AbsOff + 1] = {false, MCPhysReg(LoadDst2), 0};
                      continue;
                    }
                  }
                  // Couldn't forward — update tracking.
                  invalidateSlotReg(TRI, LoadDst1);
                  invalidateSlotReg(TRI, LoadDst2);
                  SPSlots[AbsOff] = {false, MCPhysReg(LoadDst1), 0};
                  SPSlots[AbsOff + 1] = {false, MCPhysReg(LoadDst2), 0};
                  MII = std::next(It3);
                  continue;
                }
              }
            }
            // 8-bit load: LDHL; LD r,(HL)
            // Forwarding removes LDHL (sets HL/FLAGS), so both must be dead.
            auto After8 = std::next(It1);
            if (!isRegDeadAfter(After8, MBB, TRI, Z80::HL) ||
                !isRegDeadAfter(After8, MBB, TRI, Z80::FLAGS)) {
              invalidateSlotReg(TRI, LoadDst1);
              SPSlots[AbsOff] = {false, MCPhysReg(LoadDst1), 0};
              MII = std::next(It1);
              continue;
            }
            auto AvIt = SPSlots.find(AbsOff);
            if (AvIt != SPSlots.end()) {
              SlotVal &S = AvIt->second;
              DebugLoc DL = MI.getDebugLoc();
              bool Done = false;
              if (S.IsImm) {
                unsigned LdOpc = getLDrnOpcode(LoadDst1);
                if (LdOpc) {
                  LLVM_DEBUG(dbgs() << "  SM83 fwd 8-bit imm SP+" << AbsOff
                                    << " #" << (int)S.Imm << "\n");
                  BuildMI(MBB, MI, DL, TII->get(LdOpc)).addImm(S.Imm);
                  It1->eraseFromParent();
                  MII = MBB.erase(MII);
                  Changed = true;
                  Done = true;
                }
              } else if (S.Reg != Z80::H && S.Reg != Z80::L) {
                unsigned CopyOpc =
                    (LoadDst1 == S.Reg) ? 0 : getLD8Opcode(LoadDst1, S.Reg);
                if (LoadDst1 == S.Reg || CopyOpc) {
                  LLVM_DEBUG(dbgs()
                             << "  SM83 fwd 8-bit reg SP+" << AbsOff << "\n");
                  if (CopyOpc)
                    BuildMI(MBB, MI, DL, TII->get(CopyOpc));
                  It1->eraseFromParent();
                  MII = MBB.erase(MII);
                  Changed = true;
                  Done = true;
                }
              }
              if (Done) {
                invalidateSlotReg(TRI, LoadDst1);
                SPSlots[AbsOff] = {false, MCPhysReg(LoadDst1), 0};
                continue;
              }
            }
            invalidateSlotReg(TRI, LoadDst1);
            SPSlots[AbsOff] = {false, MCPhysReg(LoadDst1), 0};
            MII = std::next(It1);
            continue;
          }

          // LDHL not followed by a recognizable pattern — HL is clobbered.
          invalidateSlotReg(TRI, Z80::HL);
          ++MII;
          continue;
        }

        // Calls and unmodeled side effects clear everything.
        if (MI.isCall() || MI.hasUnmodeledSideEffects()) {
          SPSlots.clear();
          ++MII;
          continue;
        }

        // Any other instruction: invalidate entries for defined regs.
        for (const MachineOperand &MO : MI.operands()) {
          if (MO.isReg() && MO.isDef() && MO.getReg().isPhysical())
            invalidateSlotReg(TRI, MO.getReg());
        }
        for (MCPhysReg Def : TII->get(Opc).implicit_defs())
          invalidateSlotReg(TRI, Def);
        ++MII;
      }
    }

    // --- Store-to-load forwarding and register copy elimination ---
    // Map from IX offset to the physical register holding that value.
    DenseMap<int, MCPhysReg, IXOffsetInfo> AvailValues;

    for (MachineBasicBlock::iterator MII = MBB.begin(), MIE = MBB.end();
         MII != MIE;) {
      MachineInstr &MI = *MII++;
      unsigned Opc = MI.getOpcode();

      // Case 1: IX-indexed store — LD (IX+d), R
      Register StoreSrc = getStoreIXdSrcReg(Opc);
      if (StoreSrc.isValid()) {
        int Offset = MI.getOperand(0).getImm();
        if (MI.memoperands_empty()) {
          // Spill expansion — track the value.
          AvailValues[Offset] = StoreSrc;
        } else {
          // User code (possibly volatile) — invalidate this slot.
          AvailValues.erase(Offset);
        }
        continue;
      }

      // Case 2: IX-indexed load — LD R', (IX+d)
      Register LoadDst = getLoadIXdDstReg(Opc);
      if (LoadDst.isValid()) {
        int Offset = MI.getOperand(0).getImm();

        if (MI.memoperands_empty()) {
          auto It = AvailValues.find(Offset);
          if (It != AvailValues.end()) {
            MCPhysReg SrcReg = It->second;
            if (LoadDst == SrcReg) {
              // LD R, (IX+d) where R already holds the value — no-op.
              // Don't invalidate anything: R's value doesn't change.
              LLVM_DEBUG(dbgs() << "  Eliminating redundant reload: " << MI);
              MI.eraseFromParent();
              Changed = true;
              continue;
            }
            // Replace LD R', (IX+d) with LD R', R_src.
            unsigned NewOpc = getLD8Opcode(LoadDst, SrcReg);
            if (NewOpc) {
              LLVM_DEBUG(dbgs() << "  Forwarding: " << MI << "  -> LD "
                                << printReg(LoadDst, TRI) << ", "
                                << printReg(SrcReg, TRI) << "\n");
              // R' gets a new value — invalidate other entries pointing to R'.
              invalidateReg(AvailValues, TRI, LoadDst);
              BuildMI(MBB, MI, MI.getDebugLoc(), TII->get(NewOpc));
              MI.eraseFromParent();
              Changed = true;
              AvailValues[Offset] = LoadDst;
              continue;
            }
          }
        }
        // Couldn't forward — R' gets a new value from memory.
        // Invalidate entries pointing to R' (they're stale).
        invalidateReg(AvailValues, TRI, LoadDst);
        // R' now holds the value at offset d.
        if (MI.memoperands_empty())
          AvailValues[Offset] = LoadDst;
        continue;
      }

      // Case 3: LD (IX+d), n — immediate store to IX slot
      if (Opc == Z80::LD_IXd_n) {
        int Offset = MI.getOperand(0).getImm();
        AvailValues.erase(Offset);
        continue;
      }

      // Case 4: Calls and unmodeled side effects — clear everything.
      if (MI.isCall() || MI.hasUnmodeledSideEffects()) {
        AvailValues.clear();
        continue;
      }

      // Case 5: Any other instruction — invalidate entries for defined regs.
      for (const MachineOperand &MO : MI.operands()) {
        if (MO.isReg() && MO.isDef() && MO.getReg().isPhysical())
          invalidateReg(AvailValues, TRI, MO.getReg());
      }
      // Also check implicit defs from the instruction descriptor.
      for (MCPhysReg Def : TII->get(Opc).implicit_defs())
        invalidateReg(AvailValues, TRI, Def);
    }
  }

  return Changed;
}

} // namespace

char Z80LateOptimization::ID = 0;

INITIALIZE_PASS(Z80LateOptimization, DEBUG_TYPE, "Z80 Late Optimizations",
                false, false)

MachineFunctionPass *llvm::createZ80LateOptimizationPass() {
  return new Z80LateOptimization;
}
