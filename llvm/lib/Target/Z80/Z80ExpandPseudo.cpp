//===-- Z80ExpandPseudo.cpp - Z80 Pseudo Expansion Pass -------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the Z80 pseudo instruction expansion pass.
// It handles pseudo instructions that require MBB splitting, such as
// variable shift loops (SHL8_VAR, LSHR8_VAR, etc.) which expand to
// DJNZ-based loops.
//
//===----------------------------------------------------------------------===//

#include "Z80ExpandPseudo.h"

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80.h"
#include "Z80InstrInfo.h"
#include "Z80OpcodeUtils.h"
#include "Z80Subtarget.h"

#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

#define DEBUG_TYPE "z80-expand-pseudo"

using namespace llvm;

namespace {

class Z80ExpandPseudo : public MachineFunctionPass {
public:
  static char ID;

  Z80ExpandPseudo() : MachineFunctionPass(ID) {
    llvm::initializeZ80ExpandPseudoPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  bool expandVarShift(MachineBasicBlock &MBB, MachineInstr &MI,
                      const Z80InstrInfo &TII);
  bool expandMul8(MachineBasicBlock &MBB, MachineInstr &MI,
                  const Z80InstrInfo &TII);
  bool expandUDivMod8(MachineBasicBlock &MBB, MachineInstr &MI,
                      const Z80InstrInfo &TII, bool IsDiv);
  bool expandSDivMod8(MachineBasicBlock &MBB, MachineInstr &MI,
                      const Z80InstrInfo &TII, bool IsDiv);
  bool expandSatArith8(MachineBasicBlock &MBB, MachineInstr &MI,
                       const Z80InstrInfo &TII);
  bool expandMul16(MachineBasicBlock &MBB, MachineInstr &MI,
                   const Z80InstrInfo &TII);
  bool expandUDivMod16(MachineBasicBlock &MBB, MachineInstr &MI,
                       const Z80InstrInfo &TII, bool IsDiv);
  bool expandSDivMod16(MachineBasicBlock &MBB, MachineInstr &MI,
                       const Z80InstrInfo &TII, bool IsDiv);
};

bool Z80ExpandPseudo::runOnMachineFunction(MachineFunction &MF) {
  const auto &STI = MF.getSubtarget<Z80Subtarget>();
  const auto &TII = *STI.getInstrInfo();
  bool Modified = false;

  // Iterate MBBs safely: advance iterator before processing so that
  // MBB splitting doesn't invalidate it. New MBBs inserted after the
  // current one will be visited naturally.
  for (auto MBI = MF.begin(); MBI != MF.end(); ++MBI) {
    MachineBasicBlock &MBB = *MBI;

    for (auto MI = MBB.begin(), ME = MBB.end(); MI != ME;) {
      MachineInstr &Inst = *MI;
      ++MI; // Advance before potential erase

      switch (Inst.getOpcode()) {
      case Z80::SHL8_VAR:
      case Z80::LSHR8_VAR:
      case Z80::ASHR8_VAR:
      case Z80::ROTL8_VAR:
      case Z80::ROTR8_VAR:
      case Z80::SHL16_VAR:
      case Z80::LSHR16_VAR:
      case Z80::ASHR16_VAR:
        Modified |= expandVarShift(MBB, Inst, TII);
        MI = MBB.end();
        break;
      case Z80::MUL8:
        Modified |= expandMul8(MBB, Inst, TII);
        MI = MBB.end();
        break;
      case Z80::UDIV8:
        Modified |= expandUDivMod8(MBB, Inst, TII, /*IsDiv=*/true);
        MI = MBB.end();
        break;
      case Z80::UMOD8:
        Modified |= expandUDivMod8(MBB, Inst, TII, /*IsDiv=*/false);
        MI = MBB.end();
        break;
      case Z80::SDIV8:
        Modified |= expandSDivMod8(MBB, Inst, TII, /*IsDiv=*/true);
        MI = MBB.end();
        break;
      case Z80::SMOD8:
        Modified |= expandSDivMod8(MBB, Inst, TII, /*IsDiv=*/false);
        MI = MBB.end();
        break;
      case Z80::MUL16:
        Modified |= expandMul16(MBB, Inst, TII);
        MI = MBB.end();
        break;
      case Z80::UDIV16:
        Modified |= expandUDivMod16(MBB, Inst, TII, /*IsDiv=*/true);
        MI = MBB.end();
        break;
      case Z80::UMOD16:
        Modified |= expandUDivMod16(MBB, Inst, TII, /*IsDiv=*/false);
        MI = MBB.end();
        break;
      case Z80::SDIV16:
        Modified |= expandSDivMod16(MBB, Inst, TII, /*IsDiv=*/true);
        MI = MBB.end();
        break;
      case Z80::SMOD16:
        Modified |= expandSDivMod16(MBB, Inst, TII, /*IsDiv=*/false);
        MI = MBB.end();
        break;
      case Z80::UADDSAT8:
      case Z80::USUBSAT8:
      case Z80::SADDSAT8:
      case Z80::SSUBSAT8:
        Modified |= expandSatArith8(MBB, Inst, TII);
        MI = MBB.end();
        break;
      default:
        break;
      }
    }
  }

  return Modified;
}

bool Z80ExpandPseudo::expandVarShift(MachineBasicBlock &MBB, MachineInstr &MI,
                                     const Z80InstrInfo &TII) {
  // Expand variable shift pseudo into a loop:
  //
  //   HeadMBB (original):
  //     ...
  //     inc b
  //     dec b          ; sets Z flag if B == 0, restores B
  //     jr z, TailMBB  ; skip loop if shift amount is 0
  //   LoopMBB:
  //     <shift instruction(s)>
  //     djnz LoopMBB   ; Z80: B--; loop if B != 0
  //     — or —
  //     dec b           ; SM83: B-- (SM83 lacks DJNZ)
  //     jr nz, LoopMBB  ; SM83: loop if B != 0
  //   TailMBB:
  //     ...             ; rest of original MBB

  MachineFunction *MF = MBB.getParent();
  const auto &STI = MF->getSubtarget<Z80Subtarget>();
  DebugLoc DL = MI.getDebugLoc();

  // Create new basic blocks.
  MachineBasicBlock *LoopMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *TailMBB = MF->CreateMachineBasicBlock();

  // Insert after current MBB.
  MachineFunction::iterator InsertPos = std::next(MBB.getIterator());
  MF->insert(InsertPos, LoopMBB);
  MF->insert(InsertPos, TailMBB);

  // Move everything after MI to TailMBB.
  TailMBB->splice(TailMBB->begin(), &MBB,
                  std::next(MachineBasicBlock::iterator(MI)), MBB.end());
  TailMBB->transferSuccessorsAndUpdatePHIs(&MBB);

  // HeadMBB: test B for zero and branch.
  // INC B / DEC B sets Z flag based on original B value without changing it.
  BuildMI(&MBB, DL, TII.get(Z80::INC_B));
  BuildMI(&MBB, DL, TII.get(Z80::DEC_B));
  BuildMI(&MBB, DL, TII.get(Z80::JR_Z_e)).addMBB(TailMBB);
  MBB.addSuccessor(LoopMBB);
  MBB.addSuccessor(TailMBB);

  // LoopMBB: emit shift instruction(s).
  switch (MI.getOpcode()) {
  case Z80::SHL8_VAR:
    BuildMI(LoopMBB, DL, TII.get(Z80::SLA_A));
    break;
  case Z80::LSHR8_VAR:
    BuildMI(LoopMBB, DL, TII.get(Z80::SRL_A));
    break;
  case Z80::ASHR8_VAR:
    BuildMI(LoopMBB, DL, TII.get(Z80::SRA_A));
    break;
  case Z80::ROTL8_VAR:
    BuildMI(LoopMBB, DL, TII.get(Z80::RLCA));
    break;
  case Z80::ROTR8_VAR:
    BuildMI(LoopMBB, DL, TII.get(Z80::RRCA));
    break;
  case Z80::SHL16_VAR:
    BuildMI(LoopMBB, DL, TII.get(Z80::ADD_HL_HL));
    break;
  case Z80::LSHR16_VAR:
    BuildMI(LoopMBB, DL, TII.get(Z80::SRL_H));
    BuildMI(LoopMBB, DL, TII.get(Z80::RR_L));
    break;
  case Z80::ASHR16_VAR:
    BuildMI(LoopMBB, DL, TII.get(Z80::SRA_H));
    BuildMI(LoopMBB, DL, TII.get(Z80::RR_L));
    break;
  }

  if (STI.hasSM83()) {
    // SM83 lacks DJNZ; use DEC B + JR NZ instead.
    BuildMI(LoopMBB, DL, TII.get(Z80::DEC_B));
    BuildMI(LoopMBB, DL, TII.get(Z80::JR_NZ_e)).addMBB(LoopMBB);
  } else {
    BuildMI(LoopMBB, DL, TII.get(Z80::DJNZ_e)).addMBB(LoopMBB);
  }
  LoopMBB->addSuccessor(LoopMBB); // loop back
  LoopMBB->addSuccessor(TailMBB); // fall through when done

  MI.eraseFromParent();
  return true;
}

bool Z80ExpandPseudo::expandMul8(MachineBasicBlock &MBB, MachineInstr &MI,
                                 const Z80InstrInfo &TII) {
  // Expand MUL8 pseudo into an 8-bit shift-add multiply loop.
  // Input: A = multiplier, E = multiplicand
  // Output: A = result (low 8 bits of multiplier * multiplicand)
  //
  //   HeadMBB:
  //     ld d, a       ; D = multiplier (will be shifted out)
  //     xor a         ; A = 0 (accumulator)
  //     ld b, #8      ; 8-bit counter
  //   LoopMBB:
  //     add a, a      ; A <<= 1
  //     rl d          ; D <<= 1, MSB -> carry
  //     jr nc, SkipMBB
  //     add a, e      ; A += multiplicand
  //   SkipMBB:
  //     djnz LoopMBB  ; B--; loop if B != 0
  //   TailMBB:
  //     ...

  MachineFunction *MF = MBB.getParent();
  const auto &STI = MF->getSubtarget<Z80Subtarget>();
  DebugLoc DL = MI.getDebugLoc();

  MachineBasicBlock *LoopMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *SkipMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *TailMBB = MF->CreateMachineBasicBlock();

  MachineFunction::iterator InsertPos = std::next(MBB.getIterator());
  MF->insert(InsertPos, LoopMBB);
  MF->insert(InsertPos, SkipMBB);
  MF->insert(InsertPos, TailMBB);

  // Move everything after MI to TailMBB.
  TailMBB->splice(TailMBB->begin(), &MBB,
                  std::next(MachineBasicBlock::iterator(MI)), MBB.end());
  TailMBB->transferSuccessorsAndUpdatePHIs(&MBB);

  // HeadMBB: setup
  BuildMI(&MBB, DL, TII.get(Z80::LD_D_A));           // D = multiplier
  BuildMI(&MBB, DL, TII.get(Z80::XOR_A));            // A = 0
  BuildMI(&MBB, DL, TII.get(Z80::LD_B_n)).addImm(8); // B = 8
  MBB.addSuccessor(LoopMBB);

  // LoopMBB: shift and conditionally add
  BuildMI(LoopMBB, DL, TII.get(Z80::ADD_A_A)); // A <<= 1
  BuildMI(LoopMBB, DL, TII.get(Z80::RL_D));    // D <<= 1, MSB -> carry
  BuildMI(LoopMBB, DL, TII.get(Z80::JR_NC_e)).addMBB(SkipMBB);
  BuildMI(LoopMBB, DL, TII.get(Z80::ADD_A_E)); // A += multiplicand
  LoopMBB->addSuccessor(SkipMBB);              // jr nc taken
  LoopMBB->addSuccessor(SkipMBB);              // fall through (after add)

  // SkipMBB: loop back
  if (STI.hasSM83()) {
    BuildMI(SkipMBB, DL, TII.get(Z80::DEC_B));
    BuildMI(SkipMBB, DL, TII.get(Z80::JR_NZ_e)).addMBB(LoopMBB);
  } else {
    BuildMI(SkipMBB, DL, TII.get(Z80::DJNZ_e)).addMBB(LoopMBB);
  }
  SkipMBB->addSuccessor(LoopMBB); // loop back
  SkipMBB->addSuccessor(TailMBB); // fall through when done

  MI.eraseFromParent();
  return true;
}

bool Z80ExpandPseudo::expandUDivMod8(MachineBasicBlock &MBB, MachineInstr &MI,
                                     const Z80InstrInfo &TII, bool IsDiv) {
  // Expand UDIV8/UMOD8 pseudo into an 8-bit restoring division loop.
  // Input: A = dividend, E = divisor
  // Output: A = quotient (UDIV8) or remainder (UMOD8)
  //
  //   HeadMBB:
  //     ld d, a       ; D = dividend (shifted out as quotient bits)
  //     xor a         ; A = 0 (remainder)
  //     ld b, #8      ; 8-bit counter
  //   LoopMBB:
  //     sla d         ; shift dividend MSB into carry
  //     rla           ; remainder = remainder*2 + carry
  //     cp e          ; compare remainder with divisor
  //     jr c, SkipMBB ; if remainder < divisor, skip
  //     sub e         ; remainder -= divisor
  //     inc d         ; set quotient bit 0
  //   SkipMBB:
  //     djnz LoopMBB
  //   TailMBB:
  //     ld a, d       ; (UDIV8 only: move quotient to A)

  MachineFunction *MF = MBB.getParent();
  const auto &STI = MF->getSubtarget<Z80Subtarget>();
  DebugLoc DL = MI.getDebugLoc();

  MachineBasicBlock *LoopMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *SkipMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *TailMBB = MF->CreateMachineBasicBlock();

  MachineFunction::iterator InsertPos = std::next(MBB.getIterator());
  MF->insert(InsertPos, LoopMBB);
  MF->insert(InsertPos, SkipMBB);
  MF->insert(InsertPos, TailMBB);

  TailMBB->splice(TailMBB->begin(), &MBB,
                  std::next(MachineBasicBlock::iterator(MI)), MBB.end());
  TailMBB->transferSuccessorsAndUpdatePHIs(&MBB);

  // HeadMBB: setup
  BuildMI(&MBB, DL, TII.get(Z80::LD_D_A)); // D = dividend
  BuildMI(&MBB, DL, TII.get(Z80::XOR_A));  // A = 0 (remainder)
  BuildMI(&MBB, DL, TII.get(Z80::LD_B_n)).addImm(8);
  MBB.addSuccessor(LoopMBB);

  // LoopMBB: restoring division step
  BuildMI(LoopMBB, DL, TII.get(Z80::SLA_D)); // shift dividend, MSB->carry
  BuildMI(LoopMBB, DL, TII.get(Z80::RLA));   // remainder = remainder*2 + carry
  BuildMI(LoopMBB, DL, TII.get(Z80::CP_E));  // compare remainder vs divisor
  BuildMI(LoopMBB, DL, TII.get(Z80::JR_C_e)).addMBB(SkipMBB);
  BuildMI(LoopMBB, DL, TII.get(Z80::SUB_E)); // remainder -= divisor
  BuildMI(LoopMBB, DL, TII.get(Z80::INC_D)); // set quotient bit
  LoopMBB->addSuccessor(SkipMBB);            // jr c taken
  LoopMBB->addSuccessor(SkipMBB);            // fall through

  // SkipMBB: loop back
  if (STI.hasSM83()) {
    BuildMI(SkipMBB, DL, TII.get(Z80::DEC_B));
    BuildMI(SkipMBB, DL, TII.get(Z80::JR_NZ_e)).addMBB(LoopMBB);
  } else {
    BuildMI(SkipMBB, DL, TII.get(Z80::DJNZ_e)).addMBB(LoopMBB);
  }
  SkipMBB->addSuccessor(LoopMBB);
  SkipMBB->addSuccessor(TailMBB);

  // TailMBB: for UDIV8, move quotient from D to A
  if (IsDiv) {
    BuildMI(*TailMBB, TailMBB->begin(), DL, TII.get(Z80::LD_A_D));
  }
  // For UMOD8, remainder is already in A

  MI.eraseFromParent();
  return true;
}

bool Z80ExpandPseudo::expandSDivMod8(MachineBasicBlock &MBB, MachineInstr &MI,
                                     const Z80InstrInfo &TII, bool IsDiv) {
  // Expand SDIV8/SMOD8 pseudo into sign-handling + unsigned restoring division.
  // Input: A = dividend, E = divisor
  // Output: A = quotient (SDIV8) or remainder (SMOD8)
  //
  // Register allocation: B = loop counter (DJNZ), C = sign info, D = working
  // quotient
  //   SDIV: C = dividend XOR divisor (quotient sign in bit 7)
  //   SMOD: C = original dividend (remainder sign in bit 7)
  //
  //   HeadMBB:
  //     SDIV: xor e; ld c, a; xor e  (C = dvd XOR dsr, A = dvd restored)
  //     SMOD: ld c, a                 (C = original dividend)
  //     or a / bit 7, a
  //     jp p / jr z, DvdPosMBB
  //   NegDvdMBB:
  //     neg / cpl + inc a
  //   DvdPosMBB:
  //     ld d, a          ; D = |dividend|
  //     bit 7, e
  //     jr z, DsrPosMBB
  //   NegDsrMBB:
  //     xor a; sub e; ld e, a  ; E = |divisor|
  //   DsrPosMBB:
  //     xor a            ; A = 0 (remainder)
  //     ld b, #8         ; loop counter
  //   LoopMBB:
  //     sla d; rla; cp e
  //     jr c, SkipMBB
  //   SubIncMBB:
  //     sub e; inc d
  //   SkipMBB:
  //     djnz LoopMBB
  //   SignMBB:
  //     SDIV: ld a, d; bit 7, c; jr z, TailMBB
  //     SMOD: bit 7, c; jr z, TailMBB
  //   NegResMBB:
  //     neg / cpl + inc a
  //   TailMBB:
  //     (result in A)

  MachineFunction *MF = MBB.getParent();
  const auto &STI = MF->getSubtarget<Z80Subtarget>();
  DebugLoc DL = MI.getDebugLoc();
  bool IsSM83 = STI.hasSM83();

  MachineBasicBlock *NegDvdMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *DvdPosMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *NegDsrMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *DsrPosMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *LoopMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *SubIncMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *SkipMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *SignMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *NegResMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *TailMBB = MF->CreateMachineBasicBlock();

  MachineFunction::iterator InsertPos = std::next(MBB.getIterator());
  MF->insert(InsertPos, NegDvdMBB);
  MF->insert(InsertPos, DvdPosMBB);
  MF->insert(InsertPos, NegDsrMBB);
  MF->insert(InsertPos, DsrPosMBB);
  MF->insert(InsertPos, LoopMBB);
  MF->insert(InsertPos, SubIncMBB);
  MF->insert(InsertPos, SkipMBB);
  MF->insert(InsertPos, SignMBB);
  MF->insert(InsertPos, NegResMBB);
  MF->insert(InsertPos, TailMBB);

  TailMBB->splice(TailMBB->begin(), &MBB,
                  std::next(MachineBasicBlock::iterator(MI)), MBB.end());
  TailMBB->transferSuccessorsAndUpdatePHIs(&MBB);

  // HeadMBB: save sign info to C, test dividend sign
  if (IsDiv) {
    // C = dividend XOR divisor (quotient sign in bit 7)
    BuildMI(&MBB, DL, TII.get(Z80::XOR_E));  // A = dividend XOR divisor
    BuildMI(&MBB, DL, TII.get(Z80::LD_C_A)); // C = XOR result
    BuildMI(&MBB, DL,
            TII.get(Z80::XOR_E)); // A = dividend (XOR is self-inverse)
  } else {
    // C = original dividend (remainder sign in bit 7)
    BuildMI(&MBB, DL, TII.get(Z80::LD_C_A)); // C = dividend
  }
  if (IsSM83) {
    // SM83: no JP P — use BIT 7,A + JR Z (jump if positive)
    BuildMI(&MBB, DL, TII.get(Z80::BIT_7_A));
    BuildMI(&MBB, DL, TII.get(Z80::JR_Z_e)).addMBB(DvdPosMBB);
  } else {
    BuildMI(&MBB, DL, TII.get(Z80::OR_A)); // set flags for sign test
    BuildMI(&MBB, DL, TII.get(Z80::JP_P_nn)).addMBB(DvdPosMBB);
  }
  MBB.addSuccessor(DvdPosMBB); // branch taken (positive)
  MBB.addSuccessor(NegDvdMBB); // fall through (negative)

  // NegDvdMBB: negate dividend
  if (IsSM83) {
    BuildMI(NegDvdMBB, DL, TII.get(Z80::CPL));
    BuildMI(NegDvdMBB, DL, TII.get(Z80::INC_A));
  } else {
    BuildMI(NegDvdMBB, DL, TII.get(Z80::NEG));
  }
  NegDvdMBB->addSuccessor(DvdPosMBB); // fall through

  // DvdPosMBB: save |dividend|, test divisor sign
  BuildMI(DvdPosMBB, DL, TII.get(Z80::LD_D_A)); // D = |dividend|
  BuildMI(DvdPosMBB, DL, TII.get(Z80::BIT_7_E));
  BuildMI(DvdPosMBB, DL, TII.get(Z80::JR_Z_e)).addMBB(DsrPosMBB);
  DvdPosMBB->addSuccessor(DsrPosMBB); // jr z taken (positive)
  DvdPosMBB->addSuccessor(NegDsrMBB); // fall through (negative)

  // NegDsrMBB: negate divisor
  BuildMI(NegDsrMBB, DL, TII.get(Z80::XOR_A));
  BuildMI(NegDsrMBB, DL, TII.get(Z80::SUB_E));  // A = -divisor
  BuildMI(NegDsrMBB, DL, TII.get(Z80::LD_E_A)); // E = |divisor|
  NegDsrMBB->addSuccessor(DsrPosMBB); // fall through

  // DsrPosMBB: setup for unsigned division loop
  BuildMI(DsrPosMBB, DL, TII.get(Z80::XOR_A));            // A = 0 (remainder)
  BuildMI(DsrPosMBB, DL, TII.get(Z80::LD_B_n)).addImm(8); // B = loop counter
  DsrPosMBB->addSuccessor(LoopMBB);

  // LoopMBB: restoring division step — shift and compare
  BuildMI(LoopMBB, DL, TII.get(Z80::SLA_D));
  BuildMI(LoopMBB, DL, TII.get(Z80::RLA));
  BuildMI(LoopMBB, DL, TII.get(Z80::CP_E));
  BuildMI(LoopMBB, DL, TII.get(Z80::JR_C_e)).addMBB(SkipMBB);
  LoopMBB->addSuccessor(SkipMBB);    // jr c taken (remainder < divisor)
  LoopMBB->addSuccessor(SubIncMBB);  // fall through (remainder >= divisor)

  // SubIncMBB: subtract divisor, set quotient bit
  BuildMI(SubIncMBB, DL, TII.get(Z80::SUB_E));
  BuildMI(SubIncMBB, DL, TII.get(Z80::INC_D));
  SubIncMBB->addSuccessor(SkipMBB); // fall through

  // SkipMBB: loop control
  if (IsSM83) {
    BuildMI(SkipMBB, DL, TII.get(Z80::DEC_B));
    BuildMI(SkipMBB, DL, TII.get(Z80::JR_NZ_e)).addMBB(LoopMBB);
  } else {
    BuildMI(SkipMBB, DL, TII.get(Z80::DJNZ_e)).addMBB(LoopMBB);
  }
  SkipMBB->addSuccessor(LoopMBB); // loop back
  SkipMBB->addSuccessor(SignMBB);  // fall through

  // SignMBB: apply sign to result
  if (IsDiv) {
    // Quotient sign = XOR of dividend and divisor signs (saved in C bit 7)
    BuildMI(SignMBB, DL, TII.get(Z80::LD_A_D)); // A = unsigned quotient
  }
  // (SMOD: A already has remainder)
  BuildMI(SignMBB, DL, TII.get(Z80::BIT_7_C));
  BuildMI(SignMBB, DL, TII.get(Z80::JR_Z_e)).addMBB(TailMBB);
  SignMBB->addSuccessor(TailMBB);   // jr z taken (positive)
  SignMBB->addSuccessor(NegResMBB);  // fall through (negative)

  // NegResMBB: negate result
  if (IsSM83) {
    BuildMI(NegResMBB, DL, TII.get(Z80::CPL));
    BuildMI(NegResMBB, DL, TII.get(Z80::INC_A));
  } else {
    BuildMI(NegResMBB, DL, TII.get(Z80::NEG));
  }
  NegResMBB->addSuccessor(TailMBB); // fall through

  MI.eraseFromParent();
  return true;
}

static unsigned getADD8Opc(Register Reg) {
  static const unsigned T[] = {Z80::ADD_A_A, Z80::ADD_A_B, Z80::ADD_A_C,
                               Z80::ADD_A_D, Z80::ADD_A_E, Z80::ADD_A_H,
                               Z80::ADD_A_L};
  return T[Z80::gr8RegToIndex(Reg)];
}

static unsigned getSUBOpc(Register Reg) {
  static const unsigned T[] = {Z80::SUB_A, Z80::SUB_B, Z80::SUB_C, Z80::SUB_D,
                               Z80::SUB_E, Z80::SUB_H, Z80::SUB_L};
  return T[Z80::gr8RegToIndex(Reg)];
}

bool Z80ExpandPseudo::expandSatArith8(MachineBasicBlock &MBB, MachineInstr &MI,
                                      const Z80InstrInfo &TII) {
  // Expand i8 saturating arithmetic pseudos.
  //
  // UADDSAT8 src:  ADD A,src; JR NC,.done; LD A,#0xFF; .done:
  // USUBSAT8 src:  SUB src; JR NC,.done; XOR A; .done:
  // SADDSAT8 src:  ADD A,src; JP PO,.done; RLCA; SBC A,A; XOR #0x80; .done:
  // SSUBSAT8 src:  SUB src; JP PO,.done; RLCA; SBC A,A; XOR #0x80; .done:

  MachineFunction *MF = MBB.getParent();
  DebugLoc DL = MI.getDebugLoc();
  unsigned Opc = MI.getOpcode();
  Register SrcReg = MI.getOperand(0).getReg();

  bool IsAdd = (Opc == Z80::UADDSAT8 || Opc == Z80::SADDSAT8);
  bool IsSigned = (Opc == Z80::SADDSAT8 || Opc == Z80::SSUBSAT8);

  // Create the tail MBB (rest of the original block after the pseudo).
  MachineBasicBlock *TailMBB = MF->CreateMachineBasicBlock();
  MachineFunction::iterator InsertPos = std::next(MBB.getIterator());
  MF->insert(InsertPos, TailMBB);

  // Move everything after MI to TailMBB.
  TailMBB->splice(TailMBB->begin(), &MBB,
                  std::next(MachineBasicBlock::iterator(MI)), MBB.end());
  TailMBB->transferSuccessorsAndUpdatePHIs(&MBB);

  // Emit the arithmetic instruction.
  if (IsAdd)
    BuildMI(&MBB, DL, TII.get(getADD8Opc(SrcReg)));
  else
    BuildMI(&MBB, DL, TII.get(getSUBOpc(SrcReg)));

  if (IsSigned) {
    // Signed: JP PO,.done (P/V=0 means no overflow)
    BuildMI(&MBB, DL, TII.get(Z80::JP_PO_nn)).addMBB(TailMBB);

    // Create saturation MBB.
    MachineBasicBlock *SatMBB = MF->CreateMachineBasicBlock();
    MF->insert(TailMBB->getIterator(), SatMBB);

    // Saturation: RLCA; SBC A,A; XOR #0x80
    // If result was negative (S=1): RLCA puts 1 in CF, SBC A,A = 0xFF,
    //   XOR 0x80 = 0x7F (positive overflow → max positive)
    // If result was positive (S=0): RLCA puts 0 in CF, SBC A,A = 0x00,
    //   XOR 0x80 = 0x80 (negative overflow → min negative)
    BuildMI(SatMBB, DL, TII.get(Z80::RLCA));
    BuildMI(SatMBB, DL, TII.get(Z80::SBC_A_A));
    BuildMI(SatMBB, DL, TII.get(Z80::XOR_n)).addImm(0x80);
    SatMBB->addSuccessor(TailMBB);

    MBB.addSuccessor(TailMBB); // JP PO taken (no overflow)
    MBB.addSuccessor(SatMBB);  // fall through (overflow)
  } else {
    // Unsigned: JR NC,.done (no carry = no saturation)
    BuildMI(&MBB, DL, TII.get(Z80::JR_NC_e)).addMBB(TailMBB);

    // Create saturation MBB.
    MachineBasicBlock *SatMBB = MF->CreateMachineBasicBlock();
    MF->insert(TailMBB->getIterator(), SatMBB);

    if (IsAdd) {
      // UADDSAT: saturate to 0xFF
      BuildMI(SatMBB, DL, TII.get(Z80::LD_A_n)).addImm(0xFF);
    } else {
      // USUBSAT: saturate to 0x00
      BuildMI(SatMBB, DL, TII.get(Z80::XOR_A));
    }
    SatMBB->addSuccessor(TailMBB);

    MBB.addSuccessor(TailMBB); // JR NC taken (no saturation)
    MBB.addSuccessor(SatMBB);  // fall through (saturation)
  }

  MI.eraseFromParent();
  return true;
}

bool Z80ExpandPseudo::expandMul16(MachineBasicBlock &MBB, MachineInstr &MI,
                                  const Z80InstrInfo &TII) {
  // Expand MUL16 pseudo into a 16-bit shift-add multiply loop.
  // Input: HL = multiplicand, DE = multiplier
  // Output: DE = result (low 16 bits)
  //
  // Z80 algorithm (MSB-first, matches __mulhi3):
  //   HeadMBB:
  //     ld b, d       ; BC = multiplier (save before overwrite)
  //     ld c, e
  //     ex de, hl     ; DE = multiplicand
  //     ld a, b       ; A = multiplier high (before B is overwritten)
  //     ld h, #0
  //     ld l, #0      ; HL = 0 (result accumulator)
  //     ld b, #16     ; counter (overwrites saved multiplier high)
  //   LoopMBB:
  //     add hl, hl    ; result <<= 1
  //     rl c          ; shift multiplier low left, MSB → carry
  //     rla           ; shift into A (carries from C), MSB → carry
  //     jr nc, SkipMBB
  //   AddMBB:
  //     add hl, de    ; result += multiplicand
  //   SkipMBB:
  //     djnz LoopMBB
  //   TailMBB:
  //     ex de, hl     ; DE = result
  //
  // SM83 algorithm (LSB-first, no EX DE,HL/DJNZ):
  //   HeadMBB:
  //     ld b, h       ; BC = multiplicand (from HL)
  //     ld c, l
  //     ld h, #0
  //     ld l, h       ; HL = 0 (result accumulator)
  //     ld a, #16     ; counter
  //   LoopMBB:
  //     srl d         ; DE >>= 1, LSB → carry
  //     rr e
  //     jr nc, SkipMBB
  //   AddMBB:
  //     add hl, bc    ; result += multiplicand
  //   SkipMBB:
  //     sla c         ; BC <<= 1
  //     rl b
  //     dec a
  //     jr nz, LoopMBB
  //   TailMBB:
  //     ld d, h       ; DE = result (SM83 has no EX DE,HL)
  //     ld e, l

  MachineFunction *MF = MBB.getParent();
  const auto &STI = MF->getSubtarget<Z80Subtarget>();
  DebugLoc DL = MI.getDebugLoc();
  bool IsSM83 = STI.hasSM83();

  MachineBasicBlock *LoopMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *AddMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *SkipMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *TailMBB = MF->CreateMachineBasicBlock();

  MachineFunction::iterator InsertPos = std::next(MBB.getIterator());
  MF->insert(InsertPos, LoopMBB);
  MF->insert(InsertPos, AddMBB);
  MF->insert(InsertPos, SkipMBB);
  MF->insert(InsertPos, TailMBB);

  TailMBB->splice(TailMBB->begin(), &MBB,
                  std::next(MachineBasicBlock::iterator(MI)), MBB.end());
  TailMBB->transferSuccessorsAndUpdatePHIs(&MBB);

  if (IsSM83) {
    // SM83 setup
    BuildMI(&MBB, DL, TII.get(Z80::LD_B_H));           // BC = multiplicand
    BuildMI(&MBB, DL, TII.get(Z80::LD_C_L));
    BuildMI(&MBB, DL, TII.get(Z80::LD_H_n)).addImm(0); // HL = 0
    BuildMI(&MBB, DL, TII.get(Z80::LD_L_n)).addImm(0);
    BuildMI(&MBB, DL, TII.get(Z80::LD_A_n)).addImm(16); // A = counter
  } else {
    // Z80 setup
    // The shift register is A:C (16-bit). A must hold the multiplier high byte,
    // not 0. The runtime __mulhi3 achieves this via "or b" (A = A | B = 0 | high).
    BuildMI(&MBB, DL, TII.get(Z80::LD_B_D));            // B = multiplier high
    BuildMI(&MBB, DL, TII.get(Z80::LD_C_E));            // C = multiplier low
    BuildMI(&MBB, DL, TII.get(Z80::EX_DE_HL));          // DE = multiplicand
    BuildMI(&MBB, DL, TII.get(Z80::LD_A_B));            // A = multiplier high byte
    BuildMI(&MBB, DL, TII.get(Z80::LD_H_n)).addImm(0);
    BuildMI(&MBB, DL, TII.get(Z80::LD_L_n)).addImm(0); // HL = 0
    BuildMI(&MBB, DL, TII.get(Z80::LD_B_n)).addImm(16); // B = counter
  }
  MBB.addSuccessor(LoopMBB);

  if (IsSM83) {
    // SM83 loop: LSB-first shift-and-add
    BuildMI(LoopMBB, DL, TII.get(Z80::SRL_D));          // DE >>= 1
    BuildMI(LoopMBB, DL, TII.get(Z80::RR_E));           // LSB → carry
    BuildMI(LoopMBB, DL, TII.get(Z80::JR_NC_e)).addMBB(SkipMBB);
    LoopMBB->addSuccessor(SkipMBB);                      // jr nc taken
    LoopMBB->addSuccessor(AddMBB);                       // fall through

    // AddMBB: conditional addition
    BuildMI(AddMBB, DL, TII.get(Z80::ADD_HL_BC));       // result += multiplicand
    AddMBB->addSuccessor(SkipMBB);                       // fall through

    // SM83 skip: shift multiplicand left, loop control
    BuildMI(SkipMBB, DL, TII.get(Z80::SLA_C));          // BC <<= 1
    BuildMI(SkipMBB, DL, TII.get(Z80::RL_B));
    BuildMI(SkipMBB, DL, TII.get(Z80::DEC_A));
    BuildMI(SkipMBB, DL, TII.get(Z80::JR_NZ_e)).addMBB(LoopMBB);
  } else {
    // Z80 loop: MSB-first shift-and-add
    BuildMI(LoopMBB, DL, TII.get(Z80::ADD_HL_HL));      // result <<= 1
    BuildMI(LoopMBB, DL, TII.get(Z80::RL_C));           // shift multiplier
    BuildMI(LoopMBB, DL, TII.get(Z80::RLA));            // carry propagates
    BuildMI(LoopMBB, DL, TII.get(Z80::JR_NC_e)).addMBB(SkipMBB);
    LoopMBB->addSuccessor(SkipMBB);                      // jr nc taken
    LoopMBB->addSuccessor(AddMBB);                       // fall through

    // AddMBB: conditional addition
    BuildMI(AddMBB, DL, TII.get(Z80::ADD_HL_DE));       // result += multiplicand
    AddMBB->addSuccessor(SkipMBB);                       // fall through

    // Z80 skip: loop control
    BuildMI(SkipMBB, DL, TII.get(Z80::DJNZ_e)).addMBB(LoopMBB);
  }
  SkipMBB->addSuccessor(LoopMBB);                        // loop back
  SkipMBB->addSuccessor(TailMBB);                        // fall through

  // TailMBB: move result to DE
  if (IsSM83) {
    BuildMI(*TailMBB, TailMBB->begin(), DL, TII.get(Z80::LD_E_L));
    BuildMI(*TailMBB, TailMBB->begin(), DL, TII.get(Z80::LD_D_H));
  } else {
    BuildMI(*TailMBB, TailMBB->begin(), DL, TII.get(Z80::EX_DE_HL));
  }

  MI.eraseFromParent();
  return true;
}

bool Z80ExpandPseudo::expandUDivMod16(MachineBasicBlock &MBB, MachineInstr &MI,
                                      const Z80InstrInfo &TII, bool IsDiv) {
  // Expand UDIV16/UMOD16 into a 16-bit restoring division loop.
  // Input: HL = dividend, DE = divisor
  // Output: DE = quotient (UDIV16) or remainder (UMOD16)
  //
  // Z80 algorithm (16-bit divisor path from __udivhi3):
  //   HeadMBB:
  //     ld b, h; ld c, l  ; BC = dividend (becomes quotient)
  //     ld hl, #0         ; HL = remainder
  //     ld a, #16         ; counter
  //   LoopMBB:
  //     sla c; rl b       ; shift BC left, MSB → carry
  //     adc hl, hl        ; remainder = remainder*2 + carry
  //     jr c, OverflowMBB ; 17-bit remainder, always >= divisor
  //     sbc hl, de        ; trial subtract (carry=0)
  //     jr nc, SetBitMBB  ; remainder >= divisor
  //     add hl, de        ; restore remainder
  //     jr NextMBB
  //   OverflowMBB:
  //     or a; sbc hl, de  ; subtract (clear carry first)
  //   SetBitMBB:
  //     inc c             ; set quotient bit 0
  //   NextMBB:
  //     dec a
  //     jr nz, LoopMBB
  //   TailMBB:
  //     UDIV: ld d,b; ld e,c    ; DE = quotient
  //     UMOD: ex de, hl         ; DE = remainder

  MachineFunction *MF = MBB.getParent();
  const auto &STI = MF->getSubtarget<Z80Subtarget>();
  DebugLoc DL = MI.getDebugLoc();
  bool IsSM83 = STI.hasSM83();

  MachineBasicBlock *LoopMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *RestoreMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *OverflowMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *SetBitMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *NextMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *TailMBB = MF->CreateMachineBasicBlock();

  // Layout order matters for fall-through:
  //   LoopMBB → (fall-through) RestoreMBB
  //   OverflowMBB → (fall-through) SetBitMBB → (fall-through) NextMBB
  MachineFunction::iterator InsertPos = std::next(MBB.getIterator());
  MF->insert(InsertPos, LoopMBB);
  MF->insert(InsertPos, RestoreMBB);
  MF->insert(InsertPos, OverflowMBB);
  MF->insert(InsertPos, SetBitMBB);
  MF->insert(InsertPos, NextMBB);
  MF->insert(InsertPos, TailMBB);

  TailMBB->splice(TailMBB->begin(), &MBB,
                  std::next(MachineBasicBlock::iterator(MI)), MBB.end());
  TailMBB->transferSuccessorsAndUpdatePHIs(&MBB);

  // HeadMBB: setup
  BuildMI(&MBB, DL, TII.get(Z80::LD_B_H));            // BC = dividend
  BuildMI(&MBB, DL, TII.get(Z80::LD_C_L));
  BuildMI(&MBB, DL, TII.get(Z80::LD_H_n)).addImm(0);  // HL = 0 (remainder)
  BuildMI(&MBB, DL, TII.get(Z80::LD_L_n)).addImm(0);
  BuildMI(&MBB, DL, TII.get(Z80::LD_A_n)).addImm(16); // A = counter
  MBB.addSuccessor(LoopMBB);

  // LoopMBB: shift dividend, extend remainder
  if (IsSM83) {
    BuildMI(LoopMBB, DL, TII.get(Z80::PUSH_AF));        // save counter
  }
  BuildMI(LoopMBB, DL, TII.get(Z80::SLA_C));            // shift BC left
  BuildMI(LoopMBB, DL, TII.get(Z80::RL_B));             // MSB → carry
  if (IsSM83) {
    // SM83: emulate ADC HL,HL (no native instruction)
    BuildMI(LoopMBB, DL, TII.get(Z80::LD_A_L));
    BuildMI(LoopMBB, DL, TII.get(Z80::ADC_A_L));
    BuildMI(LoopMBB, DL, TII.get(Z80::LD_L_A));
    BuildMI(LoopMBB, DL, TII.get(Z80::LD_A_H));
    BuildMI(LoopMBB, DL, TII.get(Z80::ADC_A_H));
    BuildMI(LoopMBB, DL, TII.get(Z80::LD_H_A));
  } else {
    BuildMI(LoopMBB, DL, TII.get(Z80::ADC_HL_HL));      // remainder*2 + carry
  }
  BuildMI(LoopMBB, DL, TII.get(Z80::JR_C_e)).addMBB(OverflowMBB);
  LoopMBB->addSuccessor(OverflowMBB);                    // jr c taken
  LoopMBB->addSuccessor(RestoreMBB);                     // fall through

  // RestoreMBB: trial subtract, check, possibly restore
  if (IsSM83) {
    // SM83: emulate SBC HL,DE (no native instruction, carry=0 here)
    BuildMI(RestoreMBB, DL, TII.get(Z80::LD_A_L));
    BuildMI(RestoreMBB, DL, TII.get(Z80::SUB_E));
    BuildMI(RestoreMBB, DL, TII.get(Z80::LD_L_A));
    BuildMI(RestoreMBB, DL, TII.get(Z80::LD_A_H));
    BuildMI(RestoreMBB, DL, TII.get(Z80::SBC_A_D));
    BuildMI(RestoreMBB, DL, TII.get(Z80::LD_H_A));
  } else {
    BuildMI(RestoreMBB, DL, TII.get(Z80::SBC_HL_DE));   // trial subtract
  }
  BuildMI(RestoreMBB, DL, TII.get(Z80::JR_NC_e)).addMBB(SetBitMBB);
  BuildMI(RestoreMBB, DL, TII.get(Z80::ADD_HL_DE));     // restore remainder
  BuildMI(RestoreMBB, DL, TII.get(Z80::JR_e)).addMBB(NextMBB);
  RestoreMBB->addSuccessor(SetBitMBB);                   // jr nc taken
  RestoreMBB->addSuccessor(NextMBB);                     // jr (restore path)

  // OverflowMBB: 17-bit remainder, always >= divisor
  if (IsSM83) {
    // SM83: subtract without SBC HL,DE (carry doesn't matter, result fits)
    BuildMI(OverflowMBB, DL, TII.get(Z80::LD_A_L));
    BuildMI(OverflowMBB, DL, TII.get(Z80::SUB_E));
    BuildMI(OverflowMBB, DL, TII.get(Z80::LD_L_A));
    BuildMI(OverflowMBB, DL, TII.get(Z80::LD_A_H));
    BuildMI(OverflowMBB, DL, TII.get(Z80::SBC_A_D));
    BuildMI(OverflowMBB, DL, TII.get(Z80::LD_H_A));
  } else {
    BuildMI(OverflowMBB, DL, TII.get(Z80::OR_A));       // clear carry
    BuildMI(OverflowMBB, DL, TII.get(Z80::SBC_HL_DE));  // subtract
  }
  OverflowMBB->addSuccessor(SetBitMBB);                  // fall through

  // SetBitMBB: set quotient bit
  BuildMI(SetBitMBB, DL, TII.get(Z80::INC_C));          // quotient bit 0
  SetBitMBB->addSuccessor(NextMBB);                      // fall through

  // NextMBB: loop control
  if (IsSM83) {
    BuildMI(NextMBB, DL, TII.get(Z80::POP_AF));         // restore counter
    BuildMI(NextMBB, DL, TII.get(Z80::DEC_A));
    BuildMI(NextMBB, DL, TII.get(Z80::JR_NZ_e)).addMBB(LoopMBB);
  } else {
    BuildMI(NextMBB, DL, TII.get(Z80::DEC_A));
    BuildMI(NextMBB, DL, TII.get(Z80::JR_NZ_e)).addMBB(LoopMBB);
  }
  NextMBB->addSuccessor(LoopMBB);                        // loop back
  NextMBB->addSuccessor(TailMBB);                        // fall through

  // TailMBB: move result to DE
  if (IsDiv) {
    // UDIV: DE = quotient (from BC)
    BuildMI(*TailMBB, TailMBB->begin(), DL, TII.get(Z80::LD_E_C));
    BuildMI(*TailMBB, TailMBB->begin(), DL, TII.get(Z80::LD_D_B));
  } else {
    // UMOD: DE = remainder (from HL)
    if (IsSM83) {
      BuildMI(*TailMBB, TailMBB->begin(), DL, TII.get(Z80::LD_E_L));
      BuildMI(*TailMBB, TailMBB->begin(), DL, TII.get(Z80::LD_D_H));
    } else {
      BuildMI(*TailMBB, TailMBB->begin(), DL, TII.get(Z80::EX_DE_HL));
    }
  }

  MI.eraseFromParent();
  return true;
}

bool Z80ExpandPseudo::expandSDivMod16(MachineBasicBlock &MBB, MachineInstr &MI,
                                      const Z80InstrInfo &TII, bool IsDiv) {
  // Expand SDIV16/SMOD16 into sign handling + inline unsigned division.
  // Input: HL = dividend, DE = divisor
  // Output: DE = quotient (SDIV16) or remainder (SMOD16)
  //
  // Strategy: make operands positive, do unsigned division, apply sign.
  //   HeadMBB:
  //     ld a, h; xor d    ; bit 7 = result sign (for SDIV)
  //     push af           ; save result sign
  //     bit 7, h; jr z, DvdPosMBB
  //     ; negate HL: xor a; sub l; ld l,a; sbc a,a; sub h; ld h,a
  //   DvdPosMBB:
  //     bit 7, d; jr z, DsrPosMBB
  //     ; negate DE: xor a; sub e; ld e,a; sbc a,a; sub d; ld d,a
  //   DsrPosMBB:
  //     <inline unsigned division>
  //   ...after division...
  //   SignMBB:
  //     pop af; bit 7, a; jr z, TailMBB
  //     ; negate DE
  //   TailMBB:
  //     result in DE

  MachineFunction *MF = MBB.getParent();
  const auto &STI = MF->getSubtarget<Z80Subtarget>();
  DebugLoc DL = MI.getDebugLoc();
  bool IsSM83 = STI.hasSM83();

  // Create all basic blocks
  MachineBasicBlock *NegDvdMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *DvdPosMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *NegDsrMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *DsrPosMBB = MF->CreateMachineBasicBlock();
  // Division loop blocks
  MachineBasicBlock *LoopMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *OverflowMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *RestoreMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *SetBitMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *NextMBB = MF->CreateMachineBasicBlock();
  // Sign application
  MachineBasicBlock *SignMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *NegResMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *TailMBB = MF->CreateMachineBasicBlock();

  // Layout order matters for fall-through:
  //   LoopMBB → (fall-through) RestoreMBB
  //   OverflowMBB → (fall-through) SetBitMBB → (fall-through) NextMBB
  MachineFunction::iterator InsertPos = std::next(MBB.getIterator());
  MF->insert(InsertPos, NegDvdMBB);
  MF->insert(InsertPos, DvdPosMBB);
  MF->insert(InsertPos, NegDsrMBB);
  MF->insert(InsertPos, DsrPosMBB);
  MF->insert(InsertPos, LoopMBB);
  MF->insert(InsertPos, RestoreMBB);
  MF->insert(InsertPos, OverflowMBB);
  MF->insert(InsertPos, SetBitMBB);
  MF->insert(InsertPos, NextMBB);
  MF->insert(InsertPos, SignMBB);
  MF->insert(InsertPos, NegResMBB);
  MF->insert(InsertPos, TailMBB);

  TailMBB->splice(TailMBB->begin(), &MBB,
                  std::next(MachineBasicBlock::iterator(MI)), MBB.end());
  TailMBB->transferSuccessorsAndUpdatePHIs(&MBB);

  // HeadMBB: determine result sign and make dividend positive
  if (IsDiv) {
    // SDIV: result sign = XOR of operand signs
    BuildMI(&MBB, DL, TII.get(Z80::LD_A_H));
    BuildMI(&MBB, DL, TII.get(Z80::XOR_D));  // bit 7 = result sign
  } else {
    // SMOD: result sign = dividend sign
    BuildMI(&MBB, DL, TII.get(Z80::LD_A_H));
  }
  BuildMI(&MBB, DL, TII.get(Z80::PUSH_AF));  // save sign info

  // Make dividend positive
  BuildMI(&MBB, DL, TII.get(Z80::BIT_7_H));
  BuildMI(&MBB, DL, TII.get(Z80::JR_Z_e)).addMBB(DvdPosMBB);
  MBB.addSuccessor(DvdPosMBB);   // jr z taken (positive)
  MBB.addSuccessor(NegDvdMBB);   // fall through (negative)

  // NegDvdMBB: negate HL
  BuildMI(NegDvdMBB, DL, TII.get(Z80::XOR_A));
  BuildMI(NegDvdMBB, DL, TII.get(Z80::SUB_L));
  BuildMI(NegDvdMBB, DL, TII.get(Z80::LD_L_A));
  BuildMI(NegDvdMBB, DL, TII.get(Z80::SBC_A_A));
  BuildMI(NegDvdMBB, DL, TII.get(Z80::SUB_H));
  BuildMI(NegDvdMBB, DL, TII.get(Z80::LD_H_A));
  NegDvdMBB->addSuccessor(DvdPosMBB);  // fall through

  // DvdPosMBB: make divisor positive
  BuildMI(DvdPosMBB, DL, TII.get(Z80::BIT_7_D));
  BuildMI(DvdPosMBB, DL, TII.get(Z80::JR_Z_e)).addMBB(DsrPosMBB);
  DvdPosMBB->addSuccessor(DsrPosMBB);  // jr z taken (positive)
  DvdPosMBB->addSuccessor(NegDsrMBB);  // fall through (negative)

  // NegDsrMBB: negate DE
  BuildMI(NegDsrMBB, DL, TII.get(Z80::XOR_A));
  BuildMI(NegDsrMBB, DL, TII.get(Z80::SUB_E));
  BuildMI(NegDsrMBB, DL, TII.get(Z80::LD_E_A));
  BuildMI(NegDsrMBB, DL, TII.get(Z80::SBC_A_A));
  BuildMI(NegDsrMBB, DL, TII.get(Z80::SUB_D));
  BuildMI(NegDsrMBB, DL, TII.get(Z80::LD_D_A));
  NegDsrMBB->addSuccessor(DsrPosMBB);  // fall through

  // DsrPosMBB: setup for unsigned division (same as UDIV16)
  BuildMI(DsrPosMBB, DL, TII.get(Z80::LD_B_H));            // BC = dividend
  BuildMI(DsrPosMBB, DL, TII.get(Z80::LD_C_L));
  BuildMI(DsrPosMBB, DL, TII.get(Z80::LD_H_n)).addImm(0);  // HL = 0
  BuildMI(DsrPosMBB, DL, TII.get(Z80::LD_L_n)).addImm(0);
  BuildMI(DsrPosMBB, DL, TII.get(Z80::LD_A_n)).addImm(16); // A = counter
  DsrPosMBB->addSuccessor(LoopMBB);

  // LoopMBB..NextMBB: same division loop as UDIV16
  if (IsSM83) {
    BuildMI(LoopMBB, DL, TII.get(Z80::PUSH_AF));
  }
  BuildMI(LoopMBB, DL, TII.get(Z80::SLA_C));
  BuildMI(LoopMBB, DL, TII.get(Z80::RL_B));
  if (IsSM83) {
    BuildMI(LoopMBB, DL, TII.get(Z80::LD_A_L));
    BuildMI(LoopMBB, DL, TII.get(Z80::ADC_A_L));
    BuildMI(LoopMBB, DL, TII.get(Z80::LD_L_A));
    BuildMI(LoopMBB, DL, TII.get(Z80::LD_A_H));
    BuildMI(LoopMBB, DL, TII.get(Z80::ADC_A_H));
    BuildMI(LoopMBB, DL, TII.get(Z80::LD_H_A));
  } else {
    BuildMI(LoopMBB, DL, TII.get(Z80::ADC_HL_HL));
  }
  BuildMI(LoopMBB, DL, TII.get(Z80::JR_C_e)).addMBB(OverflowMBB);
  LoopMBB->addSuccessor(OverflowMBB);
  LoopMBB->addSuccessor(RestoreMBB);

  if (IsSM83) {
    BuildMI(RestoreMBB, DL, TII.get(Z80::LD_A_L));
    BuildMI(RestoreMBB, DL, TII.get(Z80::SUB_E));
    BuildMI(RestoreMBB, DL, TII.get(Z80::LD_L_A));
    BuildMI(RestoreMBB, DL, TII.get(Z80::LD_A_H));
    BuildMI(RestoreMBB, DL, TII.get(Z80::SBC_A_D));
    BuildMI(RestoreMBB, DL, TII.get(Z80::LD_H_A));
  } else {
    BuildMI(RestoreMBB, DL, TII.get(Z80::SBC_HL_DE));
  }
  BuildMI(RestoreMBB, DL, TII.get(Z80::JR_NC_e)).addMBB(SetBitMBB);
  BuildMI(RestoreMBB, DL, TII.get(Z80::ADD_HL_DE));
  BuildMI(RestoreMBB, DL, TII.get(Z80::JR_e)).addMBB(NextMBB);
  RestoreMBB->addSuccessor(SetBitMBB);
  RestoreMBB->addSuccessor(NextMBB);

  if (IsSM83) {
    BuildMI(OverflowMBB, DL, TII.get(Z80::LD_A_L));
    BuildMI(OverflowMBB, DL, TII.get(Z80::SUB_E));
    BuildMI(OverflowMBB, DL, TII.get(Z80::LD_L_A));
    BuildMI(OverflowMBB, DL, TII.get(Z80::LD_A_H));
    BuildMI(OverflowMBB, DL, TII.get(Z80::SBC_A_D));
    BuildMI(OverflowMBB, DL, TII.get(Z80::LD_H_A));
  } else {
    BuildMI(OverflowMBB, DL, TII.get(Z80::OR_A));
    BuildMI(OverflowMBB, DL, TII.get(Z80::SBC_HL_DE));
  }
  OverflowMBB->addSuccessor(SetBitMBB);

  BuildMI(SetBitMBB, DL, TII.get(Z80::INC_C));
  SetBitMBB->addSuccessor(NextMBB);

  if (IsSM83) {
    BuildMI(NextMBB, DL, TII.get(Z80::POP_AF));
    BuildMI(NextMBB, DL, TII.get(Z80::DEC_A));
    BuildMI(NextMBB, DL, TII.get(Z80::JR_NZ_e)).addMBB(LoopMBB);
  } else {
    BuildMI(NextMBB, DL, TII.get(Z80::DEC_A));
    BuildMI(NextMBB, DL, TII.get(Z80::JR_NZ_e)).addMBB(LoopMBB);
  }
  NextMBB->addSuccessor(LoopMBB);
  NextMBB->addSuccessor(SignMBB);

  // SignMBB: move result to DE, apply sign
  if (IsDiv) {
    // DE = quotient (from BC)
    BuildMI(SignMBB, DL, TII.get(Z80::LD_D_B));
    BuildMI(SignMBB, DL, TII.get(Z80::LD_E_C));
  } else {
    // DE = remainder (from HL)
    if (IsSM83) {
      BuildMI(SignMBB, DL, TII.get(Z80::LD_D_H));
      BuildMI(SignMBB, DL, TII.get(Z80::LD_E_L));
    } else {
      BuildMI(SignMBB, DL, TII.get(Z80::EX_DE_HL));
    }
  }
  // Check sign and conditionally negate DE
  BuildMI(SignMBB, DL, TII.get(Z80::POP_AF));   // restore sign info
  BuildMI(SignMBB, DL, TII.get(Z80::BIT_7_A));
  BuildMI(SignMBB, DL, TII.get(Z80::JR_Z_e)).addMBB(TailMBB);
  SignMBB->addSuccessor(TailMBB);   // jr z taken (positive)
  SignMBB->addSuccessor(NegResMBB);  // fall through (negative)

  // NegResMBB: negate DE
  BuildMI(NegResMBB, DL, TII.get(Z80::XOR_A));
  BuildMI(NegResMBB, DL, TII.get(Z80::SUB_E));
  BuildMI(NegResMBB, DL, TII.get(Z80::LD_E_A));
  BuildMI(NegResMBB, DL, TII.get(Z80::SBC_A_A));
  BuildMI(NegResMBB, DL, TII.get(Z80::SUB_D));
  BuildMI(NegResMBB, DL, TII.get(Z80::LD_D_A));
  NegResMBB->addSuccessor(TailMBB);  // fall through

  MI.eraseFromParent();
  return true;
}

} // namespace

char Z80ExpandPseudo::ID = 0;

INITIALIZE_PASS(Z80ExpandPseudo, DEBUG_TYPE,
                "Expand Z80 pseudo instructions requiring MBB splitting", false,
                false)

MachineFunctionPass *llvm::createZ80ExpandPseudoPass() {
  return new Z80ExpandPseudo();
}
