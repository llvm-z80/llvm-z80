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

#include "llvm/CodeGen/LivePhysRegs.h"
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

  if (Modified) {
    // Blocks created above start with no live-in list, which every later
    // liveness query reads as "everything dead" — and several passes act on
    // that answer. Recompute the blocks that have none; a block whose
    // live-ins are genuinely empty recomputes back to empty.
    SmallVector<MachineBasicBlock *, 8> NoLiveIns;
    for (MachineBasicBlock &B : MF)
      if (&B != &MF.front() && B.livein_empty())
        NoLiveIns.push_back(&B);
    fullyRecomputeLiveIns(NoLiveIns);
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
  Z80::buildIncDec8(&MBB, DL, TII, Z80::INC_r, Z80::B);
  Z80::buildIncDec8(&MBB, DL, TII, Z80::DEC_r, Z80::B);
  BuildMI(&MBB, DL, TII.get(Z80::JR_Z_e)).addMBB(TailMBB);
  MBB.addSuccessor(LoopMBB);
  MBB.addSuccessor(TailMBB);

  // LoopMBB: emit shift instruction(s).
  switch (MI.getOpcode()) {
  case Z80::SHL8_VAR:
    Z80::buildRotate8(LoopMBB, DL, TII, Z80::SLA_r, Z80::A);
    break;
  case Z80::LSHR8_VAR:
    Z80::buildRotate8(LoopMBB, DL, TII, Z80::SRL_r, Z80::A);
    break;
  case Z80::ASHR8_VAR:
    Z80::buildRotate8(LoopMBB, DL, TII, Z80::SRA_r, Z80::A);
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
    Z80::buildRotate8(LoopMBB, DL, TII, Z80::SRL_r, Z80::H);
    Z80::buildRotate8(LoopMBB, DL, TII, Z80::RR_r, Z80::L);
    break;
  case Z80::ASHR16_VAR:
    Z80::buildRotate8(LoopMBB, DL, TII, Z80::SRA_r, Z80::H);
    Z80::buildRotate8(LoopMBB, DL, TII, Z80::RR_r, Z80::L);
    break;
  }

  if (STI.hasSM83()) {
    // SM83 lacks DJNZ; use DEC B + JR NZ instead.
    Z80::buildIncDec8(LoopMBB, DL, TII, Z80::DEC_r, Z80::B);
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
  //   AddMBB:
  //     add a, e      ; A += multiplicand
  //   SkipMBB:
  //     djnz LoopMBB  ; B--; loop if B != 0
  //   TailMBB:
  //     ...

  MachineFunction *MF = MBB.getParent();
  const auto &STI = MF->getSubtarget<Z80Subtarget>();
  DebugLoc DL = MI.getDebugLoc();

  MachineBasicBlock *LoopMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *AddMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *SkipMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *TailMBB = MF->CreateMachineBasicBlock();

  MachineFunction::iterator InsertPos = std::next(MBB.getIterator());
  MF->insert(InsertPos, LoopMBB);
  MF->insert(InsertPos, AddMBB);
  MF->insert(InsertPos, SkipMBB);
  MF->insert(InsertPos, TailMBB);

  // Move everything after MI to TailMBB.
  TailMBB->splice(TailMBB->begin(), &MBB,
                  std::next(MachineBasicBlock::iterator(MI)), MBB.end());
  TailMBB->transferSuccessorsAndUpdatePHIs(&MBB);

  // HeadMBB: setup
  Z80::buildLD8(&MBB, DL, TII, Z80::D, Z80::A);    // D = multiplier
  Z80::buildZeroA(&MBB, DL, TII);                  // A = 0
  Z80::buildLD8n(&MBB, DL, TII, Z80::B).addImm(8); // B = 8
  MBB.addSuccessor(LoopMBB);

  // LoopMBB: shift, then conditionally skip the add
  Z80::buildAlu8(LoopMBB, DL, TII, Z80::ADD_A_r, Z80::A); // A <<= 1
  Z80::buildRotate8(LoopMBB, DL, TII, Z80::RL_r,
                    Z80::D); // D <<= 1, MSB -> carry
  BuildMI(LoopMBB, DL, TII.get(Z80::JR_NC_e)).addMBB(SkipMBB);
  LoopMBB->addSuccessor(SkipMBB); // jr nc taken
  LoopMBB->addSuccessor(AddMBB);  // fall through

  // AddMBB: add the multiplicand when the shifted-out bit was set
  Z80::buildAlu8(AddMBB, DL, TII, Z80::ADD_A_r, Z80::E); // A += multiplicand
  AddMBB->addSuccessor(SkipMBB);

  // SkipMBB: loop back
  if (STI.hasSM83()) {
    Z80::buildIncDec8(SkipMBB, DL, TII, Z80::DEC_r, Z80::B);
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
  //   SubMBB:
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
  MachineBasicBlock *SubMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *SkipMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *TailMBB = MF->CreateMachineBasicBlock();

  MachineFunction::iterator InsertPos = std::next(MBB.getIterator());
  MF->insert(InsertPos, LoopMBB);
  MF->insert(InsertPos, SubMBB);
  MF->insert(InsertPos, SkipMBB);
  MF->insert(InsertPos, TailMBB);

  TailMBB->splice(TailMBB->begin(), &MBB,
                  std::next(MachineBasicBlock::iterator(MI)), MBB.end());
  TailMBB->transferSuccessorsAndUpdatePHIs(&MBB);

  // HeadMBB: setup
  Z80::buildLD8(&MBB, DL, TII, Z80::D, Z80::A); // D = dividend
  Z80::buildZeroA(&MBB, DL, TII);               // A = 0 (remainder)
  Z80::buildLD8n(&MBB, DL, TII, Z80::B).addImm(8);
  MBB.addSuccessor(LoopMBB);

  // LoopMBB: restoring division step
  Z80::buildRotate8(LoopMBB, DL, TII, Z80::SLA_r,
                    Z80::D);                 // shift dividend, MSB->carry
  BuildMI(LoopMBB, DL, TII.get(Z80::RLA));   // remainder = remainder*2 + carry
  Z80::buildAlu8(LoopMBB, DL, TII, Z80::CP_r,
                 Z80::E); // compare remainder vs divisor
  BuildMI(LoopMBB, DL, TII.get(Z80::JR_C_e)).addMBB(SkipMBB);
  LoopMBB->addSuccessor(SkipMBB); // jr c taken
  LoopMBB->addSuccessor(SubMBB);  // fall through

  // SubMBB: subtract and record the quotient bit
  Z80::buildAlu8(SubMBB, DL, TII, Z80::SUB_r, Z80::E); // remainder -= divisor
  Z80::buildIncDec8(SubMBB, DL, TII, Z80::INC_r, Z80::D); // set quotient bit
  SubMBB->addSuccessor(SkipMBB);

  // SkipMBB: loop back
  if (STI.hasSM83()) {
    Z80::buildIncDec8(SkipMBB, DL, TII, Z80::DEC_r, Z80::B);
    BuildMI(SkipMBB, DL, TII.get(Z80::JR_NZ_e)).addMBB(LoopMBB);
  } else {
    BuildMI(SkipMBB, DL, TII.get(Z80::DJNZ_e)).addMBB(LoopMBB);
  }
  SkipMBB->addSuccessor(LoopMBB);
  SkipMBB->addSuccessor(TailMBB);

  // TailMBB: for UDIV8, move quotient from D to A
  if (IsDiv) {
    Z80::buildLD8(*TailMBB, TailMBB->begin(), DL, TII, Z80::A, Z80::D);
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
    Z80::buildAlu8(&MBB, DL, TII, Z80::XOR_r,
                   Z80::E);                       // A = dividend XOR divisor
    Z80::buildLD8(&MBB, DL, TII, Z80::C, Z80::A); // C = XOR result
    // A = dividend (XOR is self-inverse)
    Z80::buildAlu8(&MBB, DL, TII, Z80::XOR_r, Z80::E);
  } else {
    // C = original dividend (remainder sign in bit 7)
    Z80::buildLD8(&MBB, DL, TII, Z80::C, Z80::A); // C = dividend
  }
  if (IsSM83) {
    // SM83: no JP P — use BIT 7,A + JR Z (jump if positive)
    Z80::buildBitTest(&MBB, DL, TII, 7, Z80::A);
    BuildMI(&MBB, DL, TII.get(Z80::JR_Z_e)).addMBB(DvdPosMBB);
  } else {
    Z80::buildAlu8(&MBB, DL, TII, Z80::OR_r, Z80::A); // set flags for sign test
    BuildMI(&MBB, DL, TII.get(Z80::JP_P_nn)).addMBB(DvdPosMBB);
  }
  MBB.addSuccessor(DvdPosMBB); // branch taken (positive)
  MBB.addSuccessor(NegDvdMBB); // fall through (negative)

  // NegDvdMBB: negate dividend
  if (IsSM83) {
    BuildMI(NegDvdMBB, DL, TII.get(Z80::CPL));
    Z80::buildIncDec8(NegDvdMBB, DL, TII, Z80::INC_r, Z80::A);
  } else {
    BuildMI(NegDvdMBB, DL, TII.get(Z80::NEG));
  }
  NegDvdMBB->addSuccessor(DvdPosMBB); // fall through

  // DvdPosMBB: save |dividend|, test divisor sign
  Z80::buildLD8(DvdPosMBB, DL, TII, Z80::D, Z80::A); // D = |dividend|
  Z80::buildBitTest(DvdPosMBB, DL, TII, 7, Z80::E);
  BuildMI(DvdPosMBB, DL, TII.get(Z80::JR_Z_e)).addMBB(DsrPosMBB);
  DvdPosMBB->addSuccessor(DsrPosMBB); // jr z taken (positive)
  DvdPosMBB->addSuccessor(NegDsrMBB); // fall through (negative)

  // NegDsrMBB: negate divisor
  Z80::buildZeroA(NegDsrMBB, DL, TII);
  Z80::buildAlu8(NegDsrMBB, DL, TII, Z80::SUB_r, Z80::E); // A = -divisor
  Z80::buildLD8(NegDsrMBB, DL, TII, Z80::E, Z80::A);      // E = |divisor|
  NegDsrMBB->addSuccessor(DsrPosMBB); // fall through

  // DsrPosMBB: setup for unsigned division loop
  Z80::buildZeroA(DsrPosMBB, DL, TII);                  // A = 0 (remainder)
  Z80::buildLD8n(DsrPosMBB, DL, TII, Z80::B).addImm(8); // B = loop counter
  DsrPosMBB->addSuccessor(LoopMBB);

  // LoopMBB: restoring division step — shift and compare
  Z80::buildRotate8(LoopMBB, DL, TII, Z80::SLA_r, Z80::D);
  BuildMI(LoopMBB, DL, TII.get(Z80::RLA));
  Z80::buildAlu8(LoopMBB, DL, TII, Z80::CP_r, Z80::E);
  BuildMI(LoopMBB, DL, TII.get(Z80::JR_C_e)).addMBB(SkipMBB);
  LoopMBB->addSuccessor(SkipMBB);    // jr c taken (remainder < divisor)
  LoopMBB->addSuccessor(SubIncMBB);  // fall through (remainder >= divisor)

  // SubIncMBB: subtract divisor, set quotient bit
  Z80::buildAlu8(SubIncMBB, DL, TII, Z80::SUB_r, Z80::E);
  Z80::buildIncDec8(SubIncMBB, DL, TII, Z80::INC_r, Z80::D);
  SubIncMBB->addSuccessor(SkipMBB); // fall through

  // SkipMBB: loop control
  if (IsSM83) {
    Z80::buildIncDec8(SkipMBB, DL, TII, Z80::DEC_r, Z80::B);
    BuildMI(SkipMBB, DL, TII.get(Z80::JR_NZ_e)).addMBB(LoopMBB);
  } else {
    BuildMI(SkipMBB, DL, TII.get(Z80::DJNZ_e)).addMBB(LoopMBB);
  }
  SkipMBB->addSuccessor(LoopMBB); // loop back
  SkipMBB->addSuccessor(SignMBB);  // fall through

  // SignMBB: apply sign to result
  if (IsDiv) {
    // Quotient sign = XOR of dividend and divisor signs (saved in C bit 7)
    Z80::buildLD8(SignMBB, DL, TII, Z80::A, Z80::D); // A = unsigned quotient
  }
  // (SMOD: A already has remainder)
  Z80::buildBitTest(SignMBB, DL, TII, 7, Z80::C);
  BuildMI(SignMBB, DL, TII.get(Z80::JR_Z_e)).addMBB(TailMBB);
  SignMBB->addSuccessor(TailMBB);   // jr z taken (positive)
  SignMBB->addSuccessor(NegResMBB);  // fall through (negative)

  // NegResMBB: negate result
  if (IsSM83) {
    BuildMI(NegResMBB, DL, TII.get(Z80::CPL));
    Z80::buildIncDec8(NegResMBB, DL, TII, Z80::INC_r, Z80::A);
  } else {
    BuildMI(NegResMBB, DL, TII.get(Z80::NEG));
  }
  NegResMBB->addSuccessor(TailMBB); // fall through

  MI.eraseFromParent();
  return true;
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
    Z80::buildAlu8(&MBB, DL, TII, Z80::ADD_A_r, SrcReg);
  else
    Z80::buildAlu8(&MBB, DL, TII, Z80::SUB_r, SrcReg);

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
    Z80::buildSbcAA(SatMBB, DL, TII);
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
      Z80::buildLD8n(SatMBB, DL, TII, Z80::A).addImm(0xFF);
    } else {
      // USUBSAT: saturate to 0x00
      Z80::buildZeroA(SatMBB, DL, TII);
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
    Z80::buildLD8(&MBB, DL, TII, Z80::B, Z80::H); // BC = multiplicand
    Z80::buildLD8(&MBB, DL, TII, Z80::C, Z80::L);
    Z80::buildLD8n(&MBB, DL, TII, Z80::H).addImm(0); // HL = 0
    Z80::buildLD8n(&MBB, DL, TII, Z80::L).addImm(0);
    Z80::buildLD8n(&MBB, DL, TII, Z80::A).addImm(16); // A = counter
  } else {
    // Z80 setup
    // The shift register is A:C (16-bit). A must hold the multiplier high byte,
    // not 0. The runtime __mulhi3 achieves this via "or b" (A = A | B = 0 | high).
    Z80::buildLD8(&MBB, DL, TII, Z80::B, Z80::D);       // B = multiplier high
    Z80::buildLD8(&MBB, DL, TII, Z80::C, Z80::E);       // C = multiplier low
    BuildMI(&MBB, DL, TII.get(Z80::EX_DE_HL));          // DE = multiplicand
    Z80::buildLD8(&MBB, DL, TII, Z80::A, Z80::B); // A = multiplier high byte
    Z80::buildLD8n(&MBB, DL, TII, Z80::H).addImm(0);
    Z80::buildLD8n(&MBB, DL, TII, Z80::L).addImm(0);  // HL = 0
    Z80::buildLD8n(&MBB, DL, TII, Z80::B).addImm(16); // B = counter
  }
  MBB.addSuccessor(LoopMBB);

  if (IsSM83) {
    // SM83 loop: LSB-first shift-and-add
    Z80::buildRotate8(LoopMBB, DL, TII, Z80::SRL_r, Z80::D); // DE >>= 1
    Z80::buildRotate8(LoopMBB, DL, TII, Z80::RR_r, Z80::E);  // LSB → carry
    BuildMI(LoopMBB, DL, TII.get(Z80::JR_NC_e)).addMBB(SkipMBB);
    LoopMBB->addSuccessor(SkipMBB);                      // jr nc taken
    LoopMBB->addSuccessor(AddMBB);                       // fall through

    // AddMBB: conditional addition
    Z80::buildAddHL(AddMBB, DL, TII, Z80::BC); // result += multiplicand
    AddMBB->addSuccessor(SkipMBB);                       // fall through

    // SM83 skip: shift multiplicand left, loop control
    Z80::buildRotate8(SkipMBB, DL, TII, Z80::SLA_r, Z80::C); // BC <<= 1
    Z80::buildRotate8(SkipMBB, DL, TII, Z80::RL_r, Z80::B);
    Z80::buildIncDec8(SkipMBB, DL, TII, Z80::DEC_r, Z80::A);
    BuildMI(SkipMBB, DL, TII.get(Z80::JR_NZ_e)).addMBB(LoopMBB);
  } else {
    // Z80 loop: MSB-first shift-and-add
    BuildMI(LoopMBB, DL, TII.get(Z80::ADD_HL_HL));      // result <<= 1
    Z80::buildRotate8(LoopMBB, DL, TII, Z80::RL_r, Z80::C); // shift multiplier
    BuildMI(LoopMBB, DL, TII.get(Z80::RLA));            // carry propagates
    BuildMI(LoopMBB, DL, TII.get(Z80::JR_NC_e)).addMBB(SkipMBB);
    LoopMBB->addSuccessor(SkipMBB);                      // jr nc taken
    LoopMBB->addSuccessor(AddMBB);                       // fall through

    // AddMBB: conditional addition
    Z80::buildAddHL(AddMBB, DL, TII, Z80::DE); // result += multiplicand
    AddMBB->addSuccessor(SkipMBB);                       // fall through

    // Z80 skip: loop control
    BuildMI(SkipMBB, DL, TII.get(Z80::DJNZ_e)).addMBB(LoopMBB);
  }
  SkipMBB->addSuccessor(LoopMBB);                        // loop back
  SkipMBB->addSuccessor(TailMBB);                        // fall through

  // TailMBB: move result to DE
  if (IsSM83) {
    Z80::buildLD8(*TailMBB, TailMBB->begin(), DL, TII, Z80::E, Z80::L);
    Z80::buildLD8(*TailMBB, TailMBB->begin(), DL, TII, Z80::D, Z80::H);
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
  Z80::buildLD8(&MBB, DL, TII, Z80::B, Z80::H); // BC = dividend
  Z80::buildLD8(&MBB, DL, TII, Z80::C, Z80::L);
  Z80::buildLD8n(&MBB, DL, TII, Z80::H).addImm(0); // HL = 0 (remainder)
  Z80::buildLD8n(&MBB, DL, TII, Z80::L).addImm(0);
  Z80::buildLD8n(&MBB, DL, TII, Z80::A).addImm(16); // A = counter
  MBB.addSuccessor(LoopMBB);

  // LoopMBB: shift dividend, extend remainder
  if (IsSM83) {
    BuildMI(LoopMBB, DL, TII.get(Z80::PUSH_AF));        // save counter
  }
  Z80::buildRotate8(LoopMBB, DL, TII, Z80::SLA_r, Z80::C); // shift BC left
  Z80::buildRotate8(LoopMBB, DL, TII, Z80::RL_r, Z80::B);  // MSB → carry
  if (IsSM83) {
    // SM83: emulate ADC HL,HL (no native instruction)
    Z80::buildLD8(LoopMBB, DL, TII, Z80::A, Z80::L);
    Z80::buildAlu8(LoopMBB, DL, TII, Z80::ADC_A_r, Z80::L);
    Z80::buildLD8(LoopMBB, DL, TII, Z80::L, Z80::A);
    Z80::buildLD8(LoopMBB, DL, TII, Z80::A, Z80::H);
    Z80::buildAlu8(LoopMBB, DL, TII, Z80::ADC_A_r, Z80::H);
    Z80::buildLD8(LoopMBB, DL, TII, Z80::H, Z80::A);
  } else {
    Z80::buildAdcSbcHL(LoopMBB, DL, TII, Z80::ADC_HL_rr,
                       Z80::HL); // remainder*2 + carry
  }
  BuildMI(LoopMBB, DL, TII.get(Z80::JR_C_e)).addMBB(OverflowMBB);
  LoopMBB->addSuccessor(OverflowMBB);                    // jr c taken
  LoopMBB->addSuccessor(RestoreMBB);                     // fall through

  // RestoreMBB: trial subtract, check, possibly restore
  if (IsSM83) {
    // SM83: emulate SBC HL,DE (no native instruction, carry=0 here)
    Z80::buildLD8(RestoreMBB, DL, TII, Z80::A, Z80::L);
    Z80::buildAlu8(RestoreMBB, DL, TII, Z80::SUB_r, Z80::E);
    Z80::buildLD8(RestoreMBB, DL, TII, Z80::L, Z80::A);
    Z80::buildLD8(RestoreMBB, DL, TII, Z80::A, Z80::H);
    Z80::buildAlu8(RestoreMBB, DL, TII, Z80::SBC_A_r, Z80::D);
    Z80::buildLD8(RestoreMBB, DL, TII, Z80::H, Z80::A);
  } else {
    Z80::buildAdcSbcHL(RestoreMBB, DL, TII, Z80::SBC_HL_rr,
                       Z80::DE); // trial subtract
  }
  BuildMI(RestoreMBB, DL, TII.get(Z80::JR_NC_e)).addMBB(SetBitMBB);
  Z80::buildAddHL(RestoreMBB, DL, TII, Z80::DE); // restore remainder
  BuildMI(RestoreMBB, DL, TII.get(Z80::JR_e)).addMBB(NextMBB);
  RestoreMBB->addSuccessor(SetBitMBB);                   // jr nc taken
  RestoreMBB->addSuccessor(NextMBB);                     // jr (restore path)

  // OverflowMBB: 17-bit remainder, always >= divisor
  if (IsSM83) {
    // SM83: subtract without SBC HL,DE (carry doesn't matter, result fits)
    Z80::buildLD8(OverflowMBB, DL, TII, Z80::A, Z80::L);
    Z80::buildAlu8(OverflowMBB, DL, TII, Z80::SUB_r, Z80::E);
    Z80::buildLD8(OverflowMBB, DL, TII, Z80::L, Z80::A);
    Z80::buildLD8(OverflowMBB, DL, TII, Z80::A, Z80::H);
    Z80::buildAlu8(OverflowMBB, DL, TII, Z80::SBC_A_r, Z80::D);
    Z80::buildLD8(OverflowMBB, DL, TII, Z80::H, Z80::A);
  } else {
    Z80::buildAlu8(OverflowMBB, DL, TII, Z80::OR_r, Z80::A); // clear carry
    Z80::buildAdcSbcHL(OverflowMBB, DL, TII, Z80::SBC_HL_rr,
                       Z80::DE); // subtract
  }
  OverflowMBB->addSuccessor(SetBitMBB);                  // fall through

  // SetBitMBB: set quotient bit
  Z80::buildIncDec8(SetBitMBB, DL, TII, Z80::INC_r, Z80::C); // quotient bit 0
  SetBitMBB->addSuccessor(NextMBB);                      // fall through

  // NextMBB: loop control
  if (IsSM83) {
    BuildMI(NextMBB, DL, TII.get(Z80::POP_AF));         // restore counter
    Z80::buildIncDec8(NextMBB, DL, TII, Z80::DEC_r, Z80::A);
    BuildMI(NextMBB, DL, TII.get(Z80::JR_NZ_e)).addMBB(LoopMBB);
  } else {
    Z80::buildIncDec8(NextMBB, DL, TII, Z80::DEC_r, Z80::A);
    BuildMI(NextMBB, DL, TII.get(Z80::JR_NZ_e)).addMBB(LoopMBB);
  }
  NextMBB->addSuccessor(LoopMBB);                        // loop back
  NextMBB->addSuccessor(TailMBB);                        // fall through

  // TailMBB: move result to DE
  if (IsDiv) {
    // UDIV: DE = quotient (from BC)
    Z80::buildLD8(*TailMBB, TailMBB->begin(), DL, TII, Z80::E, Z80::C);
    Z80::buildLD8(*TailMBB, TailMBB->begin(), DL, TII, Z80::D, Z80::B);
  } else {
    // UMOD: DE = remainder (from HL)
    if (IsSM83) {
      Z80::buildLD8(*TailMBB, TailMBB->begin(), DL, TII, Z80::E, Z80::L);
      Z80::buildLD8(*TailMBB, TailMBB->begin(), DL, TII, Z80::D, Z80::H);
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
    Z80::buildLD8(&MBB, DL, TII, Z80::A, Z80::H);
    Z80::buildAlu8(&MBB, DL, TII, Z80::XOR_r, Z80::D); // bit 7 = result sign
  } else {
    // SMOD: result sign = dividend sign
    Z80::buildLD8(&MBB, DL, TII, Z80::A, Z80::H);
  }
  BuildMI(&MBB, DL, TII.get(Z80::PUSH_AF));  // save sign info

  // Make dividend positive
  Z80::buildBitTest(&MBB, DL, TII, 7, Z80::H);
  BuildMI(&MBB, DL, TII.get(Z80::JR_Z_e)).addMBB(DvdPosMBB);
  MBB.addSuccessor(DvdPosMBB);   // jr z taken (positive)
  MBB.addSuccessor(NegDvdMBB);   // fall through (negative)

  // NegDvdMBB: negate HL
  Z80::buildZeroA(NegDvdMBB, DL, TII);
  Z80::buildAlu8(NegDvdMBB, DL, TII, Z80::SUB_r, Z80::L);
  Z80::buildLD8(NegDvdMBB, DL, TII, Z80::L, Z80::A);
  Z80::buildSbcAA(NegDvdMBB, DL, TII);
  Z80::buildAlu8(NegDvdMBB, DL, TII, Z80::SUB_r, Z80::H);
  Z80::buildLD8(NegDvdMBB, DL, TII, Z80::H, Z80::A);
  NegDvdMBB->addSuccessor(DvdPosMBB);  // fall through

  // DvdPosMBB: make divisor positive
  Z80::buildBitTest(DvdPosMBB, DL, TII, 7, Z80::D);
  BuildMI(DvdPosMBB, DL, TII.get(Z80::JR_Z_e)).addMBB(DsrPosMBB);
  DvdPosMBB->addSuccessor(DsrPosMBB);  // jr z taken (positive)
  DvdPosMBB->addSuccessor(NegDsrMBB);  // fall through (negative)

  // NegDsrMBB: negate DE
  Z80::buildZeroA(NegDsrMBB, DL, TII);
  Z80::buildAlu8(NegDsrMBB, DL, TII, Z80::SUB_r, Z80::E);
  Z80::buildLD8(NegDsrMBB, DL, TII, Z80::E, Z80::A);
  Z80::buildSbcAA(NegDsrMBB, DL, TII);
  Z80::buildAlu8(NegDsrMBB, DL, TII, Z80::SUB_r, Z80::D);
  Z80::buildLD8(NegDsrMBB, DL, TII, Z80::D, Z80::A);
  NegDsrMBB->addSuccessor(DsrPosMBB);  // fall through

  // DsrPosMBB: setup for unsigned division (same as UDIV16)
  Z80::buildLD8(DsrPosMBB, DL, TII, Z80::B, Z80::H); // BC = dividend
  Z80::buildLD8(DsrPosMBB, DL, TII, Z80::C, Z80::L);
  Z80::buildLD8n(DsrPosMBB, DL, TII, Z80::H).addImm(0); // HL = 0
  Z80::buildLD8n(DsrPosMBB, DL, TII, Z80::L).addImm(0);
  Z80::buildLD8n(DsrPosMBB, DL, TII, Z80::A).addImm(16); // A = counter
  DsrPosMBB->addSuccessor(LoopMBB);

  // LoopMBB..NextMBB: same division loop as UDIV16
  if (IsSM83) {
    BuildMI(LoopMBB, DL, TII.get(Z80::PUSH_AF));
  }
  Z80::buildRotate8(LoopMBB, DL, TII, Z80::SLA_r, Z80::C);
  Z80::buildRotate8(LoopMBB, DL, TII, Z80::RL_r, Z80::B);
  if (IsSM83) {
    Z80::buildLD8(LoopMBB, DL, TII, Z80::A, Z80::L);
    Z80::buildAlu8(LoopMBB, DL, TII, Z80::ADC_A_r, Z80::L);
    Z80::buildLD8(LoopMBB, DL, TII, Z80::L, Z80::A);
    Z80::buildLD8(LoopMBB, DL, TII, Z80::A, Z80::H);
    Z80::buildAlu8(LoopMBB, DL, TII, Z80::ADC_A_r, Z80::H);
    Z80::buildLD8(LoopMBB, DL, TII, Z80::H, Z80::A);
  } else {
    Z80::buildAdcSbcHL(LoopMBB, DL, TII, Z80::ADC_HL_rr, Z80::HL);
  }
  BuildMI(LoopMBB, DL, TII.get(Z80::JR_C_e)).addMBB(OverflowMBB);
  LoopMBB->addSuccessor(OverflowMBB);
  LoopMBB->addSuccessor(RestoreMBB);

  if (IsSM83) {
    Z80::buildLD8(RestoreMBB, DL, TII, Z80::A, Z80::L);
    Z80::buildAlu8(RestoreMBB, DL, TII, Z80::SUB_r, Z80::E);
    Z80::buildLD8(RestoreMBB, DL, TII, Z80::L, Z80::A);
    Z80::buildLD8(RestoreMBB, DL, TII, Z80::A, Z80::H);
    Z80::buildAlu8(RestoreMBB, DL, TII, Z80::SBC_A_r, Z80::D);
    Z80::buildLD8(RestoreMBB, DL, TII, Z80::H, Z80::A);
  } else {
    Z80::buildAdcSbcHL(RestoreMBB, DL, TII, Z80::SBC_HL_rr, Z80::DE);
  }
  BuildMI(RestoreMBB, DL, TII.get(Z80::JR_NC_e)).addMBB(SetBitMBB);
  Z80::buildAddHL(RestoreMBB, DL, TII, Z80::DE);
  BuildMI(RestoreMBB, DL, TII.get(Z80::JR_e)).addMBB(NextMBB);
  RestoreMBB->addSuccessor(SetBitMBB);
  RestoreMBB->addSuccessor(NextMBB);

  if (IsSM83) {
    Z80::buildLD8(OverflowMBB, DL, TII, Z80::A, Z80::L);
    Z80::buildAlu8(OverflowMBB, DL, TII, Z80::SUB_r, Z80::E);
    Z80::buildLD8(OverflowMBB, DL, TII, Z80::L, Z80::A);
    Z80::buildLD8(OverflowMBB, DL, TII, Z80::A, Z80::H);
    Z80::buildAlu8(OverflowMBB, DL, TII, Z80::SBC_A_r, Z80::D);
    Z80::buildLD8(OverflowMBB, DL, TII, Z80::H, Z80::A);
  } else {
    Z80::buildAlu8(OverflowMBB, DL, TII, Z80::OR_r, Z80::A);
    Z80::buildAdcSbcHL(OverflowMBB, DL, TII, Z80::SBC_HL_rr, Z80::DE);
  }
  OverflowMBB->addSuccessor(SetBitMBB);

  Z80::buildIncDec8(SetBitMBB, DL, TII, Z80::INC_r, Z80::C);
  SetBitMBB->addSuccessor(NextMBB);

  if (IsSM83) {
    BuildMI(NextMBB, DL, TII.get(Z80::POP_AF));
    Z80::buildIncDec8(NextMBB, DL, TII, Z80::DEC_r, Z80::A);
    BuildMI(NextMBB, DL, TII.get(Z80::JR_NZ_e)).addMBB(LoopMBB);
  } else {
    Z80::buildIncDec8(NextMBB, DL, TII, Z80::DEC_r, Z80::A);
    BuildMI(NextMBB, DL, TII.get(Z80::JR_NZ_e)).addMBB(LoopMBB);
  }
  NextMBB->addSuccessor(LoopMBB);
  NextMBB->addSuccessor(SignMBB);

  // SignMBB: move result to DE, apply sign
  if (IsDiv) {
    // DE = quotient (from BC)
    Z80::buildLD8(SignMBB, DL, TII, Z80::D, Z80::B);
    Z80::buildLD8(SignMBB, DL, TII, Z80::E, Z80::C);
  } else {
    // DE = remainder (from HL)
    if (IsSM83) {
      Z80::buildLD8(SignMBB, DL, TII, Z80::D, Z80::H);
      Z80::buildLD8(SignMBB, DL, TII, Z80::E, Z80::L);
    } else {
      BuildMI(SignMBB, DL, TII.get(Z80::EX_DE_HL));
    }
  }
  // Check sign and conditionally negate DE
  BuildMI(SignMBB, DL, TII.get(Z80::POP_AF));   // restore sign info
  Z80::buildBitTest(SignMBB, DL, TII, 7, Z80::A);
  BuildMI(SignMBB, DL, TII.get(Z80::JR_Z_e)).addMBB(TailMBB);
  SignMBB->addSuccessor(TailMBB);   // jr z taken (positive)
  SignMBB->addSuccessor(NegResMBB);  // fall through (negative)

  // NegResMBB: negate DE
  Z80::buildZeroA(NegResMBB, DL, TII);
  Z80::buildAlu8(NegResMBB, DL, TII, Z80::SUB_r, Z80::E);
  Z80::buildLD8(NegResMBB, DL, TII, Z80::E, Z80::A);
  Z80::buildSbcAA(NegResMBB, DL, TII);
  Z80::buildAlu8(NegResMBB, DL, TII, Z80::SUB_r, Z80::D);
  Z80::buildLD8(NegResMBB, DL, TII, Z80::D, Z80::A);
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
