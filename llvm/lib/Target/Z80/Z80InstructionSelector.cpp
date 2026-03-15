//===-- Z80InstructionSelector.cpp - Z80 Instruction Selector -------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the Z80 instruction selector for GlobalISel.
//
//===----------------------------------------------------------------------===//

#include "Z80InstructionSelector.h"

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80.h"
#include "Z80RegisterInfo.h"
#include "Z80Subtarget.h"

#include "llvm/CodeGen/GlobalISel/GIMatchTableExecutor.h"
#include "llvm/CodeGen/GlobalISel/GenericMachineInstrs.h"
#include "llvm/CodeGen/GlobalISel/InstructionSelector.h"
#include "llvm/CodeGen/GlobalISel/MachineIRBuilder.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetOpcodes.h"
#include "llvm/CodeGenTypes/LowLevelType.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicsZ80.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "z80-isel"

namespace {

class Z80InstructionSelector : public InstructionSelector {
public:
  Z80InstructionSelector(const Z80TargetMachine &TM, Z80Subtarget &STI,
                         Z80RegisterBankInfo &RBI);

  bool select(MachineInstr &MI) override;
  void setupGeneratedPerFunctionState(MachineFunction &MF) override {}
  static const char *getName() { return DEBUG_TYPE; }

private:
  bool selectRuntimeLibCall16(MachineInstr &MI, const char *FuncName);
  bool selectInline16(MachineInstr &MI, unsigned PseudoOpc);
  bool selectMul8(MachineInstr &MI);
  bool selectMulByConst(MachineInstr &MI);
  bool selectUDivMod8(MachineInstr &MI, bool IsDiv);
  bool selectSDivMod8(MachineInstr &MI, bool IsDiv);
  bool tryNarrowSDivMod16(MachineInstr &MI, bool IsDiv);
  void emitSigned16BitCompare(MachineBasicBlock &MBB, MachineInstr &MI,
                              Register LHS, Register RHS,
                              MachineRegisterInfo &MRI, bool InvertResult);
  bool emitFusedCompareAndBranch(MachineBasicBlock &MBB, MachineInstr &MI,
                                 MachineInstr &CmpMI, MachineRegisterInfo &MRI);
  bool emit32CompareFlags(MachineBasicBlock &MBB,
                          MachineBasicBlock::iterator InsertPt,
                          CmpInst::Predicate Pred, Register LhsLo,
                          Register LhsHi, Register RhsLo, Register RhsHi,
                          MachineRegisterInfo &MRI, const DebugLoc &DL,
                          CmpInst::Predicate &NormalizedPred,
                          bool FusedBranch = false);
  bool emit64CompareFlags(
      MachineBasicBlock &MBB, MachineBasicBlock::iterator InsertPt,
      CmpInst::Predicate Pred, Register LhsW0, Register LhsW1, Register LhsW2,
      Register LhsW3, Register RhsW0, Register RhsW1, Register RhsW2,
      Register RhsW3, MachineRegisterInfo &MRI, const DebugLoc &DL,
      CmpInst::Predicate &NormalizedPred, bool FusedBranch = false);

  /// Count foldable G_LOAD→G_ADD/SUB/PTR_ADD patterns in a BB.
  /// Used to decide if register pressure is high enough to justify folding.
  unsigned countFoldablePatternsInBB(MachineBasicBlock &MBB,
                                     MachineRegisterInfo &MRI);

  const Z80InstrInfo &TII;
  const Z80RegisterInfo &TRI;
  const Z80RegisterBankInfo &RBI;

  // Per-BB fold count cache for register pressure heuristic.
  MachineBasicBlock *CachedFoldBB = nullptr;
  unsigned CachedFoldCount = 0;
};

} // namespace

Z80InstructionSelector::Z80InstructionSelector(const Z80TargetMachine &TM,
                                               Z80Subtarget &STI,
                                               Z80RegisterBankInfo &RBI)
    : TII(*STI.getInstrInfo()), TRI(*STI.getRegisterInfo()), RBI(RBI) {}

/// Count how many 16-bit G_ADD/G_SUB/G_PTR_ADD in the BB have a single-use
/// G_LOAD from a frame index as an operand.  When this count exceeds the
/// GR16_BCDE physical register count (2), spills become likely and folding
/// into ADD_HL_FI/SUB_HL_FI is beneficial.
unsigned
Z80InstructionSelector::countFoldablePatternsInBB(MachineBasicBlock &MBB,
                                                  MachineRegisterInfo &MRI) {
  unsigned Count = 0;
  for (MachineInstr &MI : MBB) {
    unsigned Opc = MI.getOpcode();
    if (Opc != TargetOpcode::G_ADD && Opc != TargetOpcode::G_SUB &&
        Opc != TargetOpcode::G_PTR_ADD)
      continue;
    if (MRI.getType(MI.getOperand(0).getReg()).getSizeInBits() > 16)
      continue;
    // Check operands 1 and 2 (for G_ADD, either could be the load due to
    // commutativity; for G_SUB/G_PTR_ADD only operand 2).
    unsigned StartOp = (Opc == TargetOpcode::G_ADD) ? 1 : 2;
    unsigned EndOp = 2;
    for (unsigned i = StartOp; i <= EndOp; ++i) {
      Register Reg = MI.getOperand(i).getReg();
      if (!Reg.isVirtual() || !MRI.hasOneNonDBGUse(Reg))
        continue;
      MachineInstr *Def = MRI.getVRegDef(Reg);
      if (!Def || Def->getOpcode() != TargetOpcode::G_LOAD ||
          Def->getParent() != &MBB)
        continue;
      Register AddrReg = Def->getOperand(1).getReg();
      MachineInstr *AddrDef = MRI.getVRegDef(AddrReg);
      if (!AddrDef)
        continue;
      if (AddrDef->getOpcode() == TargetOpcode::G_FRAME_INDEX) {
        ++Count;
        break;
      }
      if (AddrDef->getOpcode() == TargetOpcode::G_PTR_ADD) {
        MachineInstr *Base = MRI.getVRegDef(AddrDef->getOperand(1).getReg());
        MachineInstr *Off = MRI.getVRegDef(AddrDef->getOperand(2).getReg());
        if (Base && Base->getOpcode() == TargetOpcode::G_FRAME_INDEX && Off &&
            Off->getOpcode() == TargetOpcode::G_CONSTANT) {
          ++Count;
          break;
        }
      }
    }
  }
  return Count;
}

// Emit a runtime library call for 16-bit binary ops.
// Z80:  HL=Src1, DE=Src2, result in DE  (__sdcccall(1))
// SM83: DE=Src1, BC=Src2, result in BC  (__sdcccall(1))
bool Z80InstructionSelector::selectRuntimeLibCall16(MachineInstr &MI,
                                                    const char *FuncName) {
  MachineBasicBlock &MBB = *MI.getParent();
  MachineFunction &MF = *MBB.getParent();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const auto &STI = MF.getSubtarget<Z80Subtarget>();

  Register DstReg = MI.getOperand(0).getReg();
  Register Src1Reg = MI.getOperand(1).getReg();
  Register Src2Reg = MI.getOperand(2).getReg();

  if (MRI.getType(DstReg).getSizeInBits() > 16)
    return false;

  if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI) ||
      !RBI.constrainGenericRegister(Src1Reg, Z80::GR16RegClass, MRI) ||
      !RBI.constrainGenericRegister(Src2Reg, Z80::GR16RegClass, MRI))
    return false;

  Module *M = const_cast<Module *>(MF.getFunction().getParent());
  FunctionCallee Func = M->getOrInsertFunction(
      FuncName, FunctionType::get(Type::getInt16Ty(M->getContext()),
                                  {Type::getInt16Ty(M->getContext()),
                                   Type::getInt16Ty(M->getContext())},
                                  false));
  GlobalValue *GV = cast<GlobalValue>(Func.getCallee());

  if (STI.hasSM83()) {
    // SM83: 1st arg→DE, 2nd arg→BC, return→BC
    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::DE)
        .addReg(Src1Reg);
    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::BC)
        .addReg(Src2Reg);
    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::CALL_nn))
        .addGlobalAddress(GV)
        .addUse(Z80::DE, RegState::Implicit)
        .addUse(Z80::BC, RegState::Implicit);
    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
        .addReg(Z80::BC);
  } else {
    // Z80: 1st arg→HL, 2nd arg→DE, return→DE
    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::HL)
        .addReg(Src1Reg);
    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::DE)
        .addReg(Src2Reg);
    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::CALL_nn))
        .addGlobalAddress(GV)
        .addUse(Z80::HL, RegState::Implicit)
        .addUse(Z80::DE, RegState::Implicit);
    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
        .addReg(Z80::DE);
  }

  MI.eraseFromParent();
  return true;
}

// Select 16-bit ops via inline pseudo (for +inline-i16-runtime mode).
// Input: HL = src1, DE = src2. Output: DE = result.
// The pseudo is expanded in Z80ExpandPseudo.
bool Z80InstructionSelector::selectInline16(MachineInstr &MI,
                                            unsigned PseudoOpc) {
  MachineBasicBlock &MBB = *MI.getParent();
  MachineFunction &MF = *MBB.getParent();
  MachineRegisterInfo &MRI = MF.getRegInfo();

  Register DstReg = MI.getOperand(0).getReg();
  Register Src1Reg = MI.getOperand(1).getReg();
  Register Src2Reg = MI.getOperand(2).getReg();

  if (MRI.getType(DstReg).getSizeInBits() != 16)
    return false;

  if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI) ||
      !RBI.constrainGenericRegister(Src1Reg, Z80::GR16RegClass, MRI) ||
      !RBI.constrainGenericRegister(Src2Reg, Z80::GR16RegClass, MRI))
    return false;

  const DebugLoc &DL = MI.getDebugLoc();

  // All inline 16-bit pseudos use HL=src1, DE=src2, result in DE.
  BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::HL).addReg(Src1Reg);
  BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::DE).addReg(Src2Reg);
  BuildMI(MBB, MI, DL, TII.get(PseudoOpc));
  BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg).addReg(Z80::DE);

  MI.eraseFromParent();
  return true;
}

// Select G_MUL i8: inline 8-bit shift-add multiply via MUL8 pseudo.
// Input: A = multiplier, E = multiplicand. Output: A = result.
// The MUL8 pseudo is expanded to a DJNZ loop in Z80ExpandPseudo.
bool Z80InstructionSelector::selectMul8(MachineInstr &MI) {
  MachineBasicBlock &MBB = *MI.getParent();
  MachineFunction &MF = *MBB.getParent();
  MachineRegisterInfo &MRI = MF.getRegInfo();

  Register DstReg = MI.getOperand(0).getReg();
  Register Src1Reg = MI.getOperand(1).getReg();
  Register Src2Reg = MI.getOperand(2).getReg();

  if (MRI.getType(DstReg).getSizeInBits() != 8)
    return false;

  if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI) ||
      !RBI.constrainGenericRegister(Src1Reg, Z80::GR8RegClass, MRI) ||
      !RBI.constrainGenericRegister(Src2Reg, Z80::GR8RegClass, MRI))
    return false;

  const DebugLoc &DL = MI.getDebugLoc();

  // A = multiplier (shifted left to check MSB), E = multiplicand (added)
  BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A).addReg(Src1Reg);
  BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::E).addReg(Src2Reg);
  BuildMI(MBB, MI, DL, TII.get(Z80::MUL8));
  BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg).addReg(Z80::A);

  MI.eraseFromParent();
  return true;
}

// Select G_UDIV/G_UREM i8: inline 8-bit restoring division via pseudo.
// Input: A = dividend, E = divisor. Output: A = quotient (UDIV8) or remainder
// (UMOD8). Under -Oz, emits a runtime call instead to save code size (~15B
// inline → ~10B call).
bool Z80InstructionSelector::selectUDivMod8(MachineInstr &MI, bool IsDiv) {
  MachineBasicBlock &MBB = *MI.getParent();
  MachineFunction &MF = *MBB.getParent();
  MachineRegisterInfo &MRI = MF.getRegInfo();

  Register DstReg = MI.getOperand(0).getReg();
  Register Src1Reg = MI.getOperand(1).getReg();
  Register Src2Reg = MI.getOperand(2).getReg();

  if (MRI.getType(DstReg).getSizeInBits() != 8)
    return false;

  const DebugLoc &DL = MI.getDebugLoc();

  if (MF.getFunction().hasOptSize()) {
    // -Os/-Oz: call dedicated 8-bit runtime function.
    // Convention: A = dividend, E = divisor, return A = result.
    if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI) ||
        !RBI.constrainGenericRegister(Src1Reg, Z80::GR8RegClass, MRI) ||
        !RBI.constrainGenericRegister(Src2Reg, Z80::GR8RegClass, MRI))
      return false;

    const char *FuncName = IsDiv ? "__udivqi3" : "__umodqi3";
    Module *M = const_cast<Module *>(MF.getFunction().getParent());
    FunctionCallee Func = M->getOrInsertFunction(
        FuncName, FunctionType::get(Type::getInt8Ty(M->getContext()),
                                    {Type::getInt8Ty(M->getContext()),
                                     Type::getInt8Ty(M->getContext())},
                                    false));
    GlobalValue *GV = cast<GlobalValue>(Func.getCallee());

    BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A).addReg(Src1Reg);
    BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::E).addReg(Src2Reg);
    BuildMI(MBB, MI, DL, TII.get(Z80::CALL_nn))
        .addGlobalAddress(GV)
        .addUse(Z80::A, RegState::Implicit)
        .addUse(Z80::E, RegState::Implicit);
    BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg).addReg(Z80::A);

    MI.eraseFromParent();
    return true;
  }

  if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI) ||
      !RBI.constrainGenericRegister(Src1Reg, Z80::GR8RegClass, MRI) ||
      !RBI.constrainGenericRegister(Src2Reg, Z80::GR8RegClass, MRI))
    return false;

  BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A).addReg(Src1Reg);
  BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::E).addReg(Src2Reg);
  BuildMI(MBB, MI, DL, TII.get(IsDiv ? Z80::UDIV8 : Z80::UMOD8));
  BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg).addReg(Z80::A);

  MI.eraseFromParent();
  return true;
}

// Select G_SDIV/G_SREM i8: inline 8-bit signed division via pseudo.
// Input: A = dividend, E = divisor. Output: A = quotient (SDIV8) or remainder
// (SMOD8). Under -Oz, emits a runtime call instead to save code size.
bool Z80InstructionSelector::selectSDivMod8(MachineInstr &MI, bool IsDiv) {
  MachineBasicBlock &MBB = *MI.getParent();
  MachineFunction &MF = *MBB.getParent();
  MachineRegisterInfo &MRI = MF.getRegInfo();

  Register DstReg = MI.getOperand(0).getReg();
  Register Src1Reg = MI.getOperand(1).getReg();
  Register Src2Reg = MI.getOperand(2).getReg();

  if (MRI.getType(DstReg).getSizeInBits() != 8)
    return false;

  const DebugLoc &DL = MI.getDebugLoc();

  if (MF.getFunction().hasMinSize()) {
    // -Oz: sign-extend i8 operands to i16, call __divhi3/__modhi3,
    // and truncate the result back to i8.
    if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI) ||
        !RBI.constrainGenericRegister(Src1Reg, Z80::GR8RegClass, MRI) ||
        !RBI.constrainGenericRegister(Src2Reg, Z80::GR8RegClass, MRI))
      return false;

    const char *FuncName = IsDiv ? "__divhi3" : "__modhi3";
    Module *M = const_cast<Module *>(MF.getFunction().getParent());
    FunctionCallee Func = M->getOrInsertFunction(
        FuncName, FunctionType::get(Type::getInt16Ty(M->getContext()),
                                    {Type::getInt16Ty(M->getContext()),
                                     Type::getInt16Ty(M->getContext())},
                                    false));
    GlobalValue *GV = cast<GlobalValue>(Func.getCallee());
    const auto &STI = MF.getSubtarget<Z80Subtarget>();

    if (STI.hasSM83()) {
      // SM83: 1st→DE, 2nd→BC, return→BC
      BuildMI(MBB, MI, DL, TII.get(Z80::SEXT_GR8_GR16), Z80::DE)
          .addReg(Src1Reg);
      BuildMI(MBB, MI, DL, TII.get(Z80::SEXT_GR8_GR16), Z80::BC)
          .addReg(Src2Reg);
      BuildMI(MBB, MI, DL, TII.get(Z80::CALL_nn))
          .addGlobalAddress(GV)
          .addUse(Z80::DE, RegState::Implicit)
          .addUse(Z80::BC, RegState::Implicit);
      BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg).addReg(Z80::C);
    } else {
      // Z80: 1st→HL, 2nd→DE, return→DE
      BuildMI(MBB, MI, DL, TII.get(Z80::SEXT_GR8_GR16), Z80::HL)
          .addReg(Src1Reg);
      BuildMI(MBB, MI, DL, TII.get(Z80::SEXT_GR8_GR16), Z80::DE)
          .addReg(Src2Reg);
      BuildMI(MBB, MI, DL, TII.get(Z80::CALL_nn))
          .addGlobalAddress(GV)
          .addUse(Z80::HL, RegState::Implicit)
          .addUse(Z80::DE, RegState::Implicit);
      BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg).addReg(Z80::E);
    }

    MI.eraseFromParent();
    return true;
  }

  if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI) ||
      !RBI.constrainGenericRegister(Src1Reg, Z80::GR8RegClass, MRI) ||
      !RBI.constrainGenericRegister(Src2Reg, Z80::GR8RegClass, MRI))
    return false;

  BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A).addReg(Src1Reg);
  BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::E).addReg(Src2Reg);
  BuildMI(MBB, MI, DL, TII.get(IsDiv ? Z80::SDIV8 : Z80::SMOD8));
  BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg).addReg(Z80::A);

  MI.eraseFromParent();
  return true;
}

// Try to narrow G_SDIV/G_SREM i16 to i8 when both operands are G_SEXT from i8.
// Fallback for when G_TRUNC fold doesn't apply (i16 result used directly).
bool Z80InstructionSelector::tryNarrowSDivMod16(MachineInstr &MI, bool IsDiv) {
  MachineBasicBlock &MBB = *MI.getParent();
  MachineFunction &MF = *MBB.getParent();
  MachineRegisterInfo &MRI = MF.getRegInfo();

  Register DstReg = MI.getOperand(0).getReg();
  Register Src1Reg = MI.getOperand(1).getReg();
  Register Src2Reg = MI.getOperand(2).getReg();

  if (MRI.getType(DstReg).getSizeInBits() != 16)
    return false;

  // Check if both operands come from G_SEXT i8 → i16.
  MachineInstr *Src1Def = MRI.getVRegDef(Src1Reg);
  MachineInstr *Src2Def = MRI.getVRegDef(Src2Reg);
  if (!Src1Def || !Src2Def)
    return false;
  if (Src1Def->getOpcode() != TargetOpcode::G_SEXT ||
      Src2Def->getOpcode() != TargetOpcode::G_SEXT)
    return false;

  Register Orig1 = Src1Def->getOperand(1).getReg();
  Register Orig2 = Src2Def->getOperand(1).getReg();
  if (MRI.getType(Orig1).getSizeInBits() != 8 ||
      MRI.getType(Orig2).getSizeInBits() != 8)
    return false;

  // Under -Oz, let the normal __divhi3/__modhi3 path handle it.
  if (MF.getFunction().hasMinSize())
    return false;

  const DebugLoc &DL = MI.getDebugLoc();

  // Inline 8-bit signed division, then sign-extend result to i16.
  if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI) ||
      !RBI.constrainGenericRegister(Orig1, Z80::GR8RegClass, MRI) ||
      !RBI.constrainGenericRegister(Orig2, Z80::GR8RegClass, MRI))
    return false;

  BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A).addReg(Orig1);
  BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::E).addReg(Orig2);
  BuildMI(MBB, MI, DL, TII.get(IsDiv ? Z80::SDIV8 : Z80::SMOD8));
  // Sign-extend 8-bit result in A to 16-bit destination.
  BuildMI(MBB, MI, DL, TII.get(Z80::SEXT_GR8_GR16), DstReg).addReg(Z80::A);

  MI.eraseFromParent();
  return true;
}

// Try to select G_MUL with a constant operand as inline shift-add sequence.
// This avoids the expensive __mulhi3 runtime call for small constants.
// Decomposition: x * C is expressed as a chain of ADD HL,HL (shift by 1)
// and ADD HL,rr (add original value), built by factoring out 2s and 1s.
// Example: x * 10 = ((x << 2) + x) << 1 → SHIFT,SHIFT,ADD,SHIFT
bool Z80InstructionSelector::selectMulByConst(MachineInstr &MI) {
  MachineBasicBlock &MBB = *MI.getParent();
  MachineFunction &MF = *MBB.getParent();
  MachineRegisterInfo &MRI = MF.getRegInfo();

  Register DstReg = MI.getOperand(0).getReg();
  Register Src0 = MI.getOperand(1).getReg();
  Register Src1 = MI.getOperand(2).getReg();

  if (MRI.getType(DstReg).getSizeInBits() != 16)
    return false;

  // Find the constant operand
  auto getConstVal = [&](Register Reg) -> std::optional<uint64_t> {
    MachineInstr *Def = MRI.getVRegDef(Reg);
    if (Def && Def->getOpcode() == TargetOpcode::G_CONSTANT)
      return Def->getOperand(1).getCImm()->getZExtValue() & 0xFFFF;
    return std::nullopt;
  };

  Register SrcReg;
  uint64_t C;
  if (auto Val = getConstVal(Src1)) {
    SrcReg = Src0;
    C = *Val;
  } else if (auto Val = getConstVal(Src0)) {
    SrcReg = Src1;
    C = *Val;
  } else {
    return false;
  }

  const DebugLoc &DL = MI.getDebugLoc();

  // x * 0: result is always 0.
  if (C == 0) {
    if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI))
      return false;
    BuildMI(MBB, MI, DL, TII.get(Z80::LD_HL_nn)).addImm(0);
    BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg).addReg(Z80::HL);
    MI.eraseFromParent();
    return true;
  }

  // x * 1: result is x (identity).
  if (C == 1) {
    if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(SrcReg, Z80::GR16RegClass, MRI))
      return false;
    BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg).addReg(SrcReg);
    MI.eraseFromParent();
    return true;
  }

  // Decompose C into a sequence of SHIFT (×2) and ADD_ORIG (+x) steps.
  // Algorithm: work from C down to 1:
  //   even → SHIFT, C/=2
  //   odd  → ADD_ORIG, C-=1
  // Then reverse to get execution order.
  enum StepKind { SHIFT, ADD_ORIG };
  SmallVector<StepKind, 16> Steps;
  uint64_t Remaining = C;
  while (Remaining != 1) {
    if (Remaining % 2 == 0) {
      Steps.push_back(SHIFT);
      Remaining /= 2;
    } else {
      Steps.push_back(ADD_ORIG);
      Remaining -= 1;
    }
  }
  std::reverse(Steps.begin(), Steps.end());

  // Limit: if too many steps, fall back to library call.
  // Each step is ~11 T-states; __mulhi3 is ~300+ T-states.
  if (Steps.size() > 12)
    return false;

  if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI) ||
      !RBI.constrainGenericRegister(SrcReg, Z80::GR16RegClass, MRI))
    return false;

  bool NeedOrig = llvm::any_of(Steps, [](StepKind S) { return S == ADD_ORIG; });

  // Copy source to HL
  BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::HL).addReg(SrcReg);

  // If we need original value for additions, save it in DE.
  // Don't constrain SrcReg to GR16_BCDE — it may conflict with other uses.
  if (NeedOrig) {
    BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::DE).addReg(SrcReg);
  }

  // Execute the steps
  for (auto Step : Steps) {
    if (Step == SHIFT) {
      BuildMI(MBB, MI, DL, TII.get(Z80::ADD_HL_HL));
    } else {
      BuildMI(MBB, MI, DL, TII.get(Z80::ADD_HL_DE));
    }
  }

  // Copy result from HL to dst
  BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg).addReg(Z80::HL);

  MI.eraseFromParent();
  return true;
}

// Emit 16-bit signed comparison: result in A (0 or 1).
// Computes: (sign_diff & lhs_neg) | (~sign_diff & unsigned_lt)
// LHS constrained to GR16, RHS constrained to GR16_BCDE (by caller).
// InvertResult: XOR 1 after compare (for SGE/SLE).
// Caller swaps LHS/RHS for SGT/SLE before calling.
void Z80InstructionSelector::emitSigned16BitCompare(MachineBasicBlock &MBB,
                                                    MachineInstr &MI,
                                                    Register LHS, Register RHS,
                                                    MachineRegisterInfo &MRI,
                                                    bool InvertResult) {
  const DebugLoc &DL = MI.getDebugLoc();

  // Extract high bytes into virtual registers (before SUB_HL_rr destroys HL)
  Register LhsHi = MRI.createVirtualRegister(&Z80::GR8RegClass);
  BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), LhsHi)
      .addReg(LHS, RegState{}, Z80::sub_hi);
  Register RhsHi = MRI.createVirtualRegister(&Z80::GR8RegClass);
  BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), RhsHi)
      .addReg(RHS, RegState{}, Z80::sub_hi);

  // sign_diff_mask: (LhsHi ^ RhsHi) bit7 → expand to 0xFF/0x00
  // RLCA rotates bit7 into carry; SBC A,A expands CF to 0xFF/0x00.
  // Other bits don't matter since SBC A,A overwrites A entirely.
  BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A).addReg(LhsHi);
  BuildMI(MBB, MI, DL, TII.get(Z80::XOR_r)).addReg(RhsHi);
  BuildMI(MBB, MI, DL, TII.get(Z80::RLCA));
  BuildMI(MBB, MI, DL, TII.get(Z80::SBC_A_A));
  Register SignDiffMask = MRI.createVirtualRegister(&Z80::GR8RegClass);
  BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), SignDiffMask)
      .addReg(Z80::A);

  // unsigned_lt from SUB_HL_rr carry (clobbers HL, helping regalloc in
  // high-pressure situations like i64 narrowScalar chains)
  BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::HL).addReg(LHS);
  BuildMI(MBB, MI, DL, TII.get(Z80::SUB_HL_rr)).addReg(RHS);
  BuildMI(MBB, MI, DL, TII.get(Z80::SBC_A_A));
  BuildMI(MBB, MI, DL, TII.get(Z80::AND_n)).addImm(1);
  Register UnsignedLt = MRI.createVirtualRegister(&Z80::GR8RegClass);
  BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), UnsignedLt).addReg(Z80::A);

  // ~sign_diff_mask & unsigned_lt
  BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A)
      .addReg(SignDiffMask);
  BuildMI(MBB, MI, DL, TII.get(Z80::CPL));
  BuildMI(MBB, MI, DL, TII.get(Z80::AND_r)).addReg(UnsignedLt);
  Register Part2 = MRI.createVirtualRegister(&Z80::GR8RegClass);
  BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Part2).addReg(Z80::A);

  // sign_diff & lhs_neg: extract bit7 of LhsHi as 0/1.
  // RLCA rotates bit7 into bit0; AND 1 isolates it.
  BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A).addReg(LhsHi);
  BuildMI(MBB, MI, DL, TII.get(Z80::RLCA));
  BuildMI(MBB, MI, DL, TII.get(Z80::AND_n)).addImm(1);
  BuildMI(MBB, MI, DL, TII.get(Z80::AND_r)).addReg(SignDiffMask);

  // Combine: (sign_diff & lhs_neg) | (~sign_diff & unsigned_lt)
  BuildMI(MBB, MI, DL, TII.get(Z80::OR_r)).addReg(Part2);

  if (InvertResult)
    BuildMI(MBB, MI, DL, TII.get(Z80::XOR_n)).addImm(1);
}

bool Z80InstructionSelector::emitFusedCompareAndBranch(
    MachineBasicBlock &MBB, MachineInstr &MI, MachineInstr &CmpMI,
    MachineRegisterInfo &MRI) {
  CmpInst::Predicate Pred =
      static_cast<CmpInst::Predicate>(CmpMI.getOperand(1).getPredicate());
  Register LHS = CmpMI.getOperand(2).getReg();
  Register RHS = CmpMI.getOperand(3).getReg();
  MachineBasicBlock *TargetMBB = MI.getOperand(1).getMBB();
  const LLT LHSTy = MRI.getType(LHS);
  const DebugLoc &DL = MI.getDebugLoc();

  // Normalize: convert GT/LE to LT/GE by swapping operands.
  switch (Pred) {
  case CmpInst::ICMP_UGT:
    Pred = CmpInst::ICMP_ULT;
    std::swap(LHS, RHS);
    break;
  case CmpInst::ICMP_ULE:
    Pred = CmpInst::ICMP_UGE;
    std::swap(LHS, RHS);
    break;
  case CmpInst::ICMP_SGT:
    Pred = CmpInst::ICMP_SLT;
    std::swap(LHS, RHS);
    break;
  case CmpInst::ICMP_SLE:
    Pred = CmpInst::ICMP_SGE;
    std::swap(LHS, RHS);
    break;
  default:
    break;
  }

  // Select conditional jump opcode.
  unsigned JumpOpc;
  switch (Pred) {
  case CmpInst::ICMP_EQ:
    JumpOpc = Z80::JP_Z_nn;
    break;
  case CmpInst::ICMP_NE:
    JumpOpc = Z80::JP_NZ_nn;
    break;
  case CmpInst::ICMP_ULT:
  case CmpInst::ICMP_SLT:
    JumpOpc = Z80::JP_C_nn;
    break;
  case CmpInst::ICMP_UGE:
  case CmpInst::ICMP_SGE:
    JumpOpc = Z80::JP_NC_nn;
    break;
  default:
    return false;
  }

  bool IsSigned = ICmpInst::isSigned(Pred);

  if (LHSTy.getSizeInBits() <= 8) {
    if (!IsSigned) {
      // Check if comparing with a constant for optimization.
      auto getConst = [&](Register Reg) -> std::optional<int64_t> {
        MachineInstr *Def = MRI.getVRegDef(Reg);
        if (Def && Def->getOpcode() == TargetOpcode::G_CONSTANT)
          return Def->getOperand(1).getCImm()->getZExtValue();
        return std::nullopt;
      };
      // EQ/NE comparisons are symmetric; for ULT/UGE we can only use
      // immediate on RHS.
      auto ConstRHS = getConst(RHS);
      bool IsEqNe = (Pred == CmpInst::ICMP_EQ || Pred == CmpInst::ICMP_NE);
      auto ConstLHS = IsEqNe ? getConst(LHS) : std::nullopt;

      Register VarReg = LHS;
      std::optional<int64_t> ConstVal = ConstRHS;
      if (!ConstVal && ConstLHS) {
        VarReg = RHS;
        ConstVal = ConstLHS;
      }

      if (ConstVal) {
        if (!RBI.constrainGenericRegister(VarReg, Z80::GR8RegClass, MRI))
          return false;
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A)
            .addReg(VarReg);
        if (*ConstVal == 0 && IsEqNe) {
          // Compare with 0: OR A sets Z flag (1 byte, 4T)
          BuildMI(MBB, MI, DL, TII.get(Z80::OR_r)).addReg(Z80::A);
        } else {
          // Compare with immediate: CP n (2 bytes, 7T)
          BuildMI(MBB, MI, DL, TII.get(Z80::CP_n)).addImm(*ConstVal & 0xFF);
        }
      } else {
        if (!RBI.constrainGenericRegister(LHS, Z80::GR8RegClass, MRI) ||
            !RBI.constrainGenericRegister(RHS, Z80::GR8RegClass, MRI))
          return false;
        // Unsigned/eq/ne: CP compares A with operand, sets Z and C flags.
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A).addReg(LHS);
        BuildMI(MBB, MI, DL, TII.get(Z80::CP_r)).addReg(RHS);
      }
    } else {
      // Signed: XOR 0x80 converts signed to unsigned domain, then CP.
      BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A).addReg(RHS);
      BuildMI(MBB, MI, DL, TII.get(Z80::XOR_n)).addImm(0x80);
      Register ModRHS = MRI.createVirtualRegister(&Z80::GR8RegClass);
      BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), ModRHS).addReg(Z80::A);
      BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A).addReg(LHS);
      BuildMI(MBB, MI, DL, TII.get(Z80::XOR_n)).addImm(0x80);
      BuildMI(MBB, MI, DL, TII.get(Z80::CP_r)).addReg(ModRHS);
    }
  } else if (LHSTy.getSizeInBits() <= 16) {
    const auto &STI = MBB.getParent()->getSubtarget<Z80Subtarget>();
    if (Pred == CmpInst::ICMP_EQ || Pred == CmpInst::ICMP_NE) {
      // Check if either operand is a small constant (0-255) for optimized
      // comparison. For constant C with high byte 0:
      //   C==0: LD A, L; OR H          (3 bytes, 12T)
      //   C>0:  LD A, L; SUB C; OR H   (5 bytes, 19T)
      // vs generic: LD DE,#C; AND A; SBC HL,DE (8 bytes, 37T)
      // Z flag is set iff the 16-bit value equals C.
      Register VarReg;
      int64_t ConstVal = -1;
      auto getSmallConst = [&](Register Reg) -> bool {
        MachineInstr *Def = MRI.getVRegDef(Reg);
        if (!Def || Def->getOpcode() != TargetOpcode::G_CONSTANT)
          return false;
        int64_t Val = Def->getOperand(1).getCImm()->getSExtValue();
        if (Val >= 0 && Val <= 255) {
          ConstVal = Val;
          return true;
        }
        return false;
      };
      if (getSmallConst(RHS))
        VarReg = LHS;
      else if (getSmallConst(LHS))
        VarReg = RHS;

      if (VarReg.isValid() && !STI.hasSM83()) {
        // Z80: Optimized small-constant EQ/NE test via SUB+OR.
        if (!RBI.constrainGenericRegister(VarReg, Z80::GR16RegClass, MRI))
          return false;
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::HL)
            .addReg(VarReg);
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A)
            .addReg(Z80::L);
        if (ConstVal != 0)
          BuildMI(MBB, MI, DL, TII.get(Z80::SUB_n)).addImm(ConstVal);
        BuildMI(MBB, MI, DL, TII.get(Z80::OR_r)).addReg(Z80::H);
      } else if (STI.hasSM83()) {
        // SM83: XOR-based comparison sets Z flag correctly for 16-bit EQ/NE.
        if (!RBI.constrainGenericRegister(LHS, Z80::GR16RegClass, MRI) ||
            !RBI.constrainGenericRegister(RHS, Z80::GR16RegClass, MRI))
          return false;
        BuildMI(MBB, MI, DL, TII.get(Z80::SM83_CMP_Z16))
            .addReg(LHS)
            .addReg(RHS);
      } else {
        // Z80: XOR-based 16-bit EQ/NE — avoids clobbering HL and doesn't
        // need BC/DE for constants, reducing register pressure.
        //   LD A, lhs_hi; XOR rhs_hi; LD tmp, A;
        //   LD A, lhs_lo; XOR rhs_lo; OR tmp   → Z set iff equal
        if (!RBI.constrainGenericRegister(LHS, Z80::GR16RegClass, MRI))
          return false;

        // Check if RHS is a constant for immediate XOR optimization.
        MachineInstr *RHSDef = MRI.getVRegDef(RHS);
        int64_t CVal = -1;
        if (RHSDef && RHSDef->getOpcode() == TargetOpcode::G_CONSTANT)
          CVal = RHSDef->getOperand(1).getCImm()->getZExtValue() & 0xFFFF;

        Register TmpReg = MRI.createVirtualRegister(&Z80::GR8RegClass);

        if (CVal >= 0) {
          uint8_t Lo = CVal & 0xFF;
          uint8_t Hi = (CVal >> 8) & 0xFF;
          // High byte: LD A, lhs_hi; XOR #Hi
          BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A)
              .addReg(LHS, RegState{}, Z80::sub_hi);
          if (Hi)
            BuildMI(MBB, MI, DL, TII.get(Z80::XOR_n)).addImm(Hi);
          BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), TmpReg)
              .addReg(Z80::A);
          // Low byte: LD A, lhs_lo; XOR #Lo; OR tmp
          BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A)
              .addReg(LHS, RegState{}, Z80::sub_lo);
          if (Lo)
            BuildMI(MBB, MI, DL, TII.get(Z80::XOR_n)).addImm(Lo);
          BuildMI(MBB, MI, DL, TII.get(Z80::OR_r)).addReg(TmpReg);
        } else {
          // Variable RHS: XOR with register sub-bytes.
          if (!RBI.constrainGenericRegister(RHS, Z80::GR16RegClass, MRI))
            return false;
          Register RhsHi = MRI.createVirtualRegister(&Z80::GR8RegClass);
          Register RhsLo = MRI.createVirtualRegister(&Z80::GR8RegClass);
          BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), RhsHi)
              .addReg(RHS, RegState{}, Z80::sub_hi);
          BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), RhsLo)
              .addReg(RHS, RegState{}, Z80::sub_lo);
          // High byte
          BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A)
              .addReg(LHS, RegState{}, Z80::sub_hi);
          BuildMI(MBB, MI, DL, TII.get(Z80::XOR_r)).addReg(RhsHi);
          BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), TmpReg)
              .addReg(Z80::A);
          // Low byte
          BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A)
              .addReg(LHS, RegState{}, Z80::sub_lo);
          BuildMI(MBB, MI, DL, TII.get(Z80::XOR_r)).addReg(RhsLo);
          BuildMI(MBB, MI, DL, TII.get(Z80::OR_r)).addReg(TmpReg);
        }
      }
    } else if (IsSigned) {
      // Special case: SLT/SGE against 0 → test sign bit directly.
      auto isConstZero = [&](Register R) -> bool {
        MachineInstr *Def = MRI.getVRegDef(R);
        if (!Def || Def->getOpcode() != TargetOpcode::G_CONSTANT)
          return false;
        return Def->getOperand(1).getCImm()->isZero();
      };
      if (isConstZero(RHS)) {
        // SLT X, 0: branch if sign bit set (bit 7 of high byte)
        // SGE X, 0: branch if sign bit clear
        if (!RBI.constrainGenericRegister(LHS, Z80::GR16RegClass, MRI))
          return false;
        Register HiByte = MRI.createVirtualRegister(&Z80::GR8RegClass);
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), HiByte)
            .addReg(LHS, RegState{}, Z80::sub_hi);
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A)
            .addReg(HiByte);
        // ADD A,A shifts bit 7 into carry
        BuildMI(MBB, MI, DL, TII.get(Z80::ADD_A_A));
        // SLT: branch on carry; SGE: branch on no carry
        JumpOpc = (Pred == CmpInst::ICMP_SLT) ? Z80::JP_C_nn : Z80::JP_NC_nn;
      } else {
        // Signed 16-bit: compute SLT boolean in A, then OR A to set Z flag.
        if (!RBI.constrainGenericRegister(LHS, Z80::GR16RegClass, MRI) ||
            !RBI.constrainGenericRegister(RHS, Z80::GR16_BCDERegClass, MRI))
          return false;
        bool Invert = (Pred == CmpInst::ICMP_SGE);
        emitSigned16BitCompare(MBB, MI, LHS, RHS, MRI, Invert);
        BuildMI(MBB, MI, DL, TII.get(Z80::OR_A));
        JumpOpc = Z80::JP_NZ_nn;
      }
    } else {
      // Unsigned ULT/UGE: CMP16_FLAGS sets carry flag.
      if (!RBI.constrainGenericRegister(LHS, Z80::GR16RegClass, MRI) ||
          !RBI.constrainGenericRegister(RHS, Z80::GR16RegClass, MRI))
        return false;
      BuildMI(MBB, MI, DL, TII.get(Z80::CMP16_FLAGS)).addReg(LHS).addReg(RHS);
    }
  } else {
    return false;
  }

  BuildMI(MBB, MI, DL, TII.get(JumpOpc)).addMBB(TargetMBB);
  MI.eraseFromParent();
  return true;
}

bool Z80InstructionSelector::emit32CompareFlags(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator InsertPt,
    CmpInst::Predicate Pred, Register LhsLo, Register LhsHi, Register RhsLo,
    Register RhsHi, MachineRegisterInfo &MRI, const DebugLoc &DL,
    CmpInst::Predicate &NormalizedPred, bool FusedBranch) {

  if (Pred == CmpInst::ICMP_EQ || Pred == CmpInst::ICMP_NE) {
    if (!RBI.constrainGenericRegister(LhsLo, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(LhsHi, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(RhsLo, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(RhsHi, Z80::GR16RegClass, MRI))
      return false;

    if (FusedBranch) {
      // Fused compare-and-branch: use XOR_CMP_Z16 (no normalize) for each half,
      // then OR to combine. Z=1 when equal, Z=0 when not.
      // Flip NormalizedPred so the caller's jump mapping works correctly:
      //   EQ → NE (caller emits JP_Z → jumps when Z=1 → equal)
      //   NE → EQ (caller emits JP_NZ → jumps when Z=0 → not equal)
      BuildMI(MBB, InsertPt, DL, TII.get(Z80::XOR_CMP_Z16))
          .addReg(LhsLo)
          .addReg(RhsLo);
      Register LoResult = MRI.createVirtualRegister(&Z80::GR8RegClass);
      BuildMI(MBB, InsertPt, DL, TII.get(TargetOpcode::COPY), LoResult)
          .addReg(Z80::A);
      BuildMI(MBB, InsertPt, DL, TII.get(Z80::XOR_CMP_Z16))
          .addReg(LhsHi)
          .addReg(RhsHi);
      BuildMI(MBB, InsertPt, DL, TII.get(Z80::OR_r)).addReg(LoResult);
      NormalizedPred =
          (Pred == CmpInst::ICMP_EQ) ? CmpInst::ICMP_NE : CmpInst::ICMP_EQ;
    } else {
      // Standalone: materialize 0/1 in A using XOR_CMP_EQ16 pairs.
      // XOR_CMP_EQ16 returns 1 if equal, 0 if not.
      // AND both halves: A = 1 only if full 32-bit values match.
      BuildMI(MBB, InsertPt, DL, TII.get(Z80::XOR_CMP_EQ16))
          .addReg(LhsLo)
          .addReg(RhsLo);
      Register LoEq = MRI.createVirtualRegister(&Z80::GR8RegClass);
      BuildMI(MBB, InsertPt, DL, TII.get(TargetOpcode::COPY), LoEq)
          .addReg(Z80::A);
      BuildMI(MBB, InsertPt, DL, TII.get(Z80::XOR_CMP_EQ16))
          .addReg(LhsHi)
          .addReg(RhsHi);
      BuildMI(MBB, InsertPt, DL, TII.get(Z80::AND_r)).addReg(LoEq);
      NormalizedPred = Pred;
    }
    return true;
  }

  // Ordering comparisons: normalize to ULT/UGE by swapping.
  bool Swap = false;
  switch (Pred) {
  case CmpInst::ICMP_UGT:
    Pred = CmpInst::ICMP_ULT;
    Swap = true;
    break;
  case CmpInst::ICMP_ULE:
    Pred = CmpInst::ICMP_UGE;
    Swap = true;
    break;
  case CmpInst::ICMP_SGT:
    Pred = CmpInst::ICMP_SLT;
    Swap = true;
    break;
  case CmpInst::ICMP_SLE:
    Pred = CmpInst::ICMP_SGE;
    Swap = true;
    break;
  default:
    break;
  }
  if (Swap) {
    std::swap(LhsLo, RhsLo);
    std::swap(LhsHi, RhsHi);
  }

  bool IsSigned = ICmpInst::isSigned(Pred);

  if (IsSigned) {
    // Convert signed to unsigned by XOR 0x80 on highest bytes.
    if (!RBI.constrainGenericRegister(LhsLo, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(LhsHi, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(RhsLo, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(RhsHi, Z80::GR16RegClass, MRI))
      return false;

    Register LhsHiHi = MRI.createVirtualRegister(&Z80::GR8RegClass);
    BuildMI(MBB, InsertPt, DL, TII.get(TargetOpcode::COPY), LhsHiHi)
        .addReg(LhsHi, RegState{}, Z80::sub_hi);
    Register LhsHiLo = MRI.createVirtualRegister(&Z80::GR8RegClass);
    BuildMI(MBB, InsertPt, DL, TII.get(TargetOpcode::COPY), LhsHiLo)
        .addReg(LhsHi, RegState{}, Z80::sub_lo);
    BuildMI(MBB, InsertPt, DL, TII.get(TargetOpcode::COPY), Z80::A)
        .addReg(LhsHiHi);
    BuildMI(MBB, InsertPt, DL, TII.get(Z80::XOR_n)).addImm(0x80);
    Register LhsFlipped = MRI.createVirtualRegister(&Z80::GR8RegClass);
    BuildMI(MBB, InsertPt, DL, TII.get(TargetOpcode::COPY), LhsFlipped)
        .addReg(Z80::A);
    Register NewLhsHi = MRI.createVirtualRegister(&Z80::GR16RegClass);
    BuildMI(MBB, InsertPt, DL, TII.get(TargetOpcode::REG_SEQUENCE), NewLhsHi)
        .addReg(LhsHiLo)
        .addImm(Z80::sub_lo)
        .addReg(LhsFlipped)
        .addImm(Z80::sub_hi);

    Register RhsHiHi = MRI.createVirtualRegister(&Z80::GR8RegClass);
    BuildMI(MBB, InsertPt, DL, TII.get(TargetOpcode::COPY), RhsHiHi)
        .addReg(RhsHi, RegState{}, Z80::sub_hi);
    Register RhsHiLo = MRI.createVirtualRegister(&Z80::GR8RegClass);
    BuildMI(MBB, InsertPt, DL, TII.get(TargetOpcode::COPY), RhsHiLo)
        .addReg(RhsHi, RegState{}, Z80::sub_lo);
    BuildMI(MBB, InsertPt, DL, TII.get(TargetOpcode::COPY), Z80::A)
        .addReg(RhsHiHi);
    BuildMI(MBB, InsertPt, DL, TII.get(Z80::XOR_n)).addImm(0x80);
    Register RhsFlipped = MRI.createVirtualRegister(&Z80::GR8RegClass);
    BuildMI(MBB, InsertPt, DL, TII.get(TargetOpcode::COPY), RhsFlipped)
        .addReg(Z80::A);
    Register NewRhsHi = MRI.createVirtualRegister(&Z80::GR16RegClass);
    BuildMI(MBB, InsertPt, DL, TII.get(TargetOpcode::REG_SEQUENCE), NewRhsHi)
        .addReg(RhsHiLo)
        .addImm(Z80::sub_lo)
        .addReg(RhsFlipped)
        .addImm(Z80::sub_hi);

    LhsHi = NewLhsHi;
    RhsHi = NewRhsHi;
    // Signed is now unsigned after XOR 0x80.
    Pred = (Pred == CmpInst::ICMP_SLT) ? CmpInst::ICMP_ULT : CmpInst::ICMP_UGE;
  } else {
    if (!RBI.constrainGenericRegister(LhsLo, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(LhsHi, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(RhsLo, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(RhsHi, Z80::GR16RegClass, MRI))
      return false;
  }

  // SUB_HL_rr (low 16 bits) + CMP16_SBC_FLAGS (high 16 bits) sets carry.
  if (!RBI.constrainGenericRegister(RhsLo, Z80::GR16_BCDERegClass, MRI))
    return false;
  BuildMI(MBB, InsertPt, DL, TII.get(TargetOpcode::COPY), Z80::HL)
      .addReg(LhsLo);
  BuildMI(MBB, InsertPt, DL, TII.get(Z80::SUB_HL_rr)).addReg(RhsLo);
  BuildMI(MBB, InsertPt, DL, TII.get(Z80::CMP16_SBC_FLAGS))
      .addReg(LhsHi)
      .addReg(RhsHi);

  NormalizedPred = Pred;
  return true;
}

bool Z80InstructionSelector::emit64CompareFlags(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator InsertPt,
    CmpInst::Predicate Pred, Register LhsW0, Register LhsW1, Register LhsW2,
    Register LhsW3, Register RhsW0, Register RhsW1, Register RhsW2,
    Register RhsW3, MachineRegisterInfo &MRI, const DebugLoc &DL,
    CmpInst::Predicate &NormalizedPred, bool FusedBranch) {

  if (Pred == CmpInst::ICMP_EQ || Pred == CmpInst::ICMP_NE) {
    if (!RBI.constrainGenericRegister(LhsW0, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(LhsW1, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(LhsW2, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(LhsW3, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(RhsW0, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(RhsW1, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(RhsW2, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(RhsW3, Z80::GR16RegClass, MRI))
      return false;

    if (FusedBranch) {
      // Fused: four XOR_CMP_Z16 + OR combines all word pairs.
      // Z=1 when all 8 bytes match.
      BuildMI(MBB, InsertPt, DL, TII.get(Z80::XOR_CMP_Z16))
          .addReg(LhsW0)
          .addReg(RhsW0);
      Register Tmp = MRI.createVirtualRegister(&Z80::GR8RegClass);
      BuildMI(MBB, InsertPt, DL, TII.get(TargetOpcode::COPY), Tmp)
          .addReg(Z80::A);

      BuildMI(MBB, InsertPt, DL, TII.get(Z80::XOR_CMP_Z16))
          .addReg(LhsW1)
          .addReg(RhsW1);
      BuildMI(MBB, InsertPt, DL, TII.get(Z80::OR_r)).addReg(Tmp);
      Tmp = MRI.createVirtualRegister(&Z80::GR8RegClass);
      BuildMI(MBB, InsertPt, DL, TII.get(TargetOpcode::COPY), Tmp)
          .addReg(Z80::A);

      BuildMI(MBB, InsertPt, DL, TII.get(Z80::XOR_CMP_Z16))
          .addReg(LhsW2)
          .addReg(RhsW2);
      BuildMI(MBB, InsertPt, DL, TII.get(Z80::OR_r)).addReg(Tmp);
      Tmp = MRI.createVirtualRegister(&Z80::GR8RegClass);
      BuildMI(MBB, InsertPt, DL, TII.get(TargetOpcode::COPY), Tmp)
          .addReg(Z80::A);

      BuildMI(MBB, InsertPt, DL, TII.get(Z80::XOR_CMP_Z16))
          .addReg(LhsW3)
          .addReg(RhsW3);
      BuildMI(MBB, InsertPt, DL, TII.get(Z80::OR_r)).addReg(Tmp);
      NormalizedPred =
          (Pred == CmpInst::ICMP_EQ) ? CmpInst::ICMP_NE : CmpInst::ICMP_EQ;
    } else {
      // Standalone: XOR_CMP_EQ16 each word pair, AND all results together.
      BuildMI(MBB, InsertPt, DL, TII.get(Z80::XOR_CMP_EQ16))
          .addReg(LhsW0)
          .addReg(RhsW0);
      Register Tmp = MRI.createVirtualRegister(&Z80::GR8RegClass);
      BuildMI(MBB, InsertPt, DL, TII.get(TargetOpcode::COPY), Tmp)
          .addReg(Z80::A);

      BuildMI(MBB, InsertPt, DL, TII.get(Z80::XOR_CMP_EQ16))
          .addReg(LhsW1)
          .addReg(RhsW1);
      BuildMI(MBB, InsertPt, DL, TII.get(Z80::AND_r)).addReg(Tmp);
      Tmp = MRI.createVirtualRegister(&Z80::GR8RegClass);
      BuildMI(MBB, InsertPt, DL, TII.get(TargetOpcode::COPY), Tmp)
          .addReg(Z80::A);

      BuildMI(MBB, InsertPt, DL, TII.get(Z80::XOR_CMP_EQ16))
          .addReg(LhsW2)
          .addReg(RhsW2);
      BuildMI(MBB, InsertPt, DL, TII.get(Z80::AND_r)).addReg(Tmp);
      Tmp = MRI.createVirtualRegister(&Z80::GR8RegClass);
      BuildMI(MBB, InsertPt, DL, TII.get(TargetOpcode::COPY), Tmp)
          .addReg(Z80::A);

      BuildMI(MBB, InsertPt, DL, TII.get(Z80::XOR_CMP_EQ16))
          .addReg(LhsW3)
          .addReg(RhsW3);
      BuildMI(MBB, InsertPt, DL, TII.get(Z80::AND_r)).addReg(Tmp);
      NormalizedPred = Pred;
    }
    return true;
  }

  // Ordering comparisons: normalize to ULT/UGE by swapping.
  bool Swap = false;
  switch (Pred) {
  case CmpInst::ICMP_UGT:
    Pred = CmpInst::ICMP_ULT;
    Swap = true;
    break;
  case CmpInst::ICMP_ULE:
    Pred = CmpInst::ICMP_UGE;
    Swap = true;
    break;
  case CmpInst::ICMP_SGT:
    Pred = CmpInst::ICMP_SLT;
    Swap = true;
    break;
  case CmpInst::ICMP_SLE:
    Pred = CmpInst::ICMP_SGE;
    Swap = true;
    break;
  default:
    break;
  }
  if (Swap) {
    std::swap(LhsW0, RhsW0);
    std::swap(LhsW1, RhsW1);
    std::swap(LhsW2, RhsW2);
    std::swap(LhsW3, RhsW3);
  }

  bool IsSigned = ICmpInst::isSigned(Pred);

  if (IsSigned) {
    // Convert signed to unsigned by XOR 0x80 on highest bytes (W3 high byte).
    if (!RBI.constrainGenericRegister(LhsW0, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(LhsW1, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(LhsW2, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(LhsW3, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(RhsW0, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(RhsW1, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(RhsW2, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(RhsW3, Z80::GR16RegClass, MRI))
      return false;

    // XOR 0x80 on LhsW3 high byte.
    Register LhsW3Hi = MRI.createVirtualRegister(&Z80::GR8RegClass);
    BuildMI(MBB, InsertPt, DL, TII.get(TargetOpcode::COPY), LhsW3Hi)
        .addReg(LhsW3, RegState{}, Z80::sub_hi);
    Register LhsW3Lo = MRI.createVirtualRegister(&Z80::GR8RegClass);
    BuildMI(MBB, InsertPt, DL, TII.get(TargetOpcode::COPY), LhsW3Lo)
        .addReg(LhsW3, RegState{}, Z80::sub_lo);
    BuildMI(MBB, InsertPt, DL, TII.get(TargetOpcode::COPY), Z80::A)
        .addReg(LhsW3Hi);
    BuildMI(MBB, InsertPt, DL, TII.get(Z80::XOR_n)).addImm(0x80);
    Register LhsFlipped = MRI.createVirtualRegister(&Z80::GR8RegClass);
    BuildMI(MBB, InsertPt, DL, TII.get(TargetOpcode::COPY), LhsFlipped)
        .addReg(Z80::A);
    Register NewLhsW3 = MRI.createVirtualRegister(&Z80::GR16RegClass);
    BuildMI(MBB, InsertPt, DL, TII.get(TargetOpcode::REG_SEQUENCE), NewLhsW3)
        .addReg(LhsW3Lo)
        .addImm(Z80::sub_lo)
        .addReg(LhsFlipped)
        .addImm(Z80::sub_hi);

    // XOR 0x80 on RhsW3 high byte.
    Register RhsW3Hi = MRI.createVirtualRegister(&Z80::GR8RegClass);
    BuildMI(MBB, InsertPt, DL, TII.get(TargetOpcode::COPY), RhsW3Hi)
        .addReg(RhsW3, RegState{}, Z80::sub_hi);
    Register RhsW3Lo = MRI.createVirtualRegister(&Z80::GR8RegClass);
    BuildMI(MBB, InsertPt, DL, TII.get(TargetOpcode::COPY), RhsW3Lo)
        .addReg(RhsW3, RegState{}, Z80::sub_lo);
    BuildMI(MBB, InsertPt, DL, TII.get(TargetOpcode::COPY), Z80::A)
        .addReg(RhsW3Hi);
    BuildMI(MBB, InsertPt, DL, TII.get(Z80::XOR_n)).addImm(0x80);
    Register RhsFlipped = MRI.createVirtualRegister(&Z80::GR8RegClass);
    BuildMI(MBB, InsertPt, DL, TII.get(TargetOpcode::COPY), RhsFlipped)
        .addReg(Z80::A);
    Register NewRhsW3 = MRI.createVirtualRegister(&Z80::GR16RegClass);
    BuildMI(MBB, InsertPt, DL, TII.get(TargetOpcode::REG_SEQUENCE), NewRhsW3)
        .addReg(RhsW3Lo)
        .addImm(Z80::sub_lo)
        .addReg(RhsFlipped)
        .addImm(Z80::sub_hi);

    LhsW3 = NewLhsW3;
    RhsW3 = NewRhsW3;
    Pred = (Pred == CmpInst::ICMP_SLT) ? CmpInst::ICMP_ULT : CmpInst::ICMP_UGE;
  } else {
    if (!RBI.constrainGenericRegister(LhsW0, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(LhsW1, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(LhsW2, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(LhsW3, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(RhsW0, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(RhsW1, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(RhsW2, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(RhsW3, Z80::GR16RegClass, MRI))
      return false;
  }

  // SUB_HL_rr (W0) + CMP16_SBC_FLAGS (W1, W2, W3) chains carry.
  if (!RBI.constrainGenericRegister(RhsW0, Z80::GR16_BCDERegClass, MRI))
    return false;
  BuildMI(MBB, InsertPt, DL, TII.get(TargetOpcode::COPY), Z80::HL)
      .addReg(LhsW0);
  BuildMI(MBB, InsertPt, DL, TII.get(Z80::SUB_HL_rr)).addReg(RhsW0);
  BuildMI(MBB, InsertPt, DL, TII.get(Z80::CMP16_SBC_FLAGS))
      .addReg(LhsW1)
      .addReg(RhsW1);
  BuildMI(MBB, InsertPt, DL, TII.get(Z80::CMP16_SBC_FLAGS))
      .addReg(LhsW2)
      .addReg(RhsW2);
  BuildMI(MBB, InsertPt, DL, TII.get(Z80::CMP16_SBC_FLAGS))
      .addReg(LhsW3)
      .addReg(RhsW3);

  NormalizedPred = Pred;
  return true;
}

bool Z80InstructionSelector::select(MachineInstr &MI) {
  MachineBasicBlock &MBB = *MI.getParent();
  MachineFunction &MF = *MBB.getParent();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const auto &STI = MF.getSubtarget<Z80Subtarget>();

  unsigned Opcode = MI.getOpcode();

  // Cache per-BB foldable pattern count for register pressure heuristic.
  // Only fold RELOAD+ADD into IX-indexed ALU when pressure is high enough
  // (>2 foldable patterns) to justify the +2B/fold cost via spill avoidance.
  if (&MBB != CachedFoldBB) {
    CachedFoldBB = &MBB;
    CachedFoldCount = countFoldablePatternsInBB(MBB, MRI);
  }

  // Helper: extract 8-bit constant from G_CONSTANT or G_UNMERGE_VALUES of
  // G_CONSTANT. Used by AND/OR/XOR immediate folding.
  auto getConst8 = [&](Register Reg) -> std::optional<int64_t> {
    MachineInstr *Def = MRI.getVRegDef(Reg);
    if (!Def)
      return std::nullopt;
    if (Def->getOpcode() == TargetOpcode::G_CONSTANT)
      return Def->getOperand(1).getCImm()->getSExtValue();
    if (Def->getOpcode() == TargetOpcode::G_UNMERGE_VALUES) {
      unsigned NumDefs = Def->getNumOperands() - 1;
      Register SrcReg = Def->getOperand(NumDefs).getReg();
      MachineInstr *SrcDef = MRI.getVRegDef(SrcReg);
      if (!SrcDef || SrcDef->getOpcode() != TargetOpcode::G_CONSTANT)
        return std::nullopt;
      uint64_t FullVal = SrcDef->getOperand(1).getCImm()->getZExtValue();
      unsigned EltBits = MRI.getType(Reg).getSizeInBits();
      for (unsigned I = 0; I < NumDefs; ++I) {
        if (Def->getOperand(I).getReg() == Reg)
          return (FullVal >> (I * EltBits)) & ((1ULL << EltBits) - 1);
      }
    }
    return std::nullopt;
  };

  // If the instruction is already selected (not a pre-isel generic), it's done.
  // Use MCInstrDesc::isPreISelOpcode() instead of isPreISelGenericOpcode() to
  // also cover target-specific generic instructions (Z80::G_Z80_ICMP32, etc.)
  // which have the PreISelOpcode flag but fall outside the TargetOpcode range.
  if (!MI.getDesc().isPreISelOpcode()) {
    // COPYs need special handling to constrain virtual register classes
    if (Opcode == TargetOpcode::COPY) {
      Register DstReg = MI.getOperand(0).getReg();
      Register SrcReg = MI.getOperand(1).getReg();

      // If destination is virtual and source is physical, constrain destination
      if (DstReg.isVirtual() && SrcReg.isPhysical()) {
        // Assign register classes explicitly for all Z80 physical registers.
        // Do NOT use getMinimalPhysRegClass — it returns synthesized
        // intersection classes (e.g., gr16_and_hli={HL}) that have too few
        // allocatable registers and cause register allocation failures.
        const TargetRegisterClass *RC;
        if (SrcReg == Z80::SP) {
          RC = &Z80::HLIRegClass;
        } else if (Z80::GR16RegClass.contains(SrcReg)) {
          RC = &Z80::GR16RegClass;
        } else if (Z80::GR8RegClass.contains(SrcReg)) {
          RC = &Z80::GR8RegClass;
        } else if (Z80::IR16RegClass.contains(SrcReg)) {
          // copyPhysReg handles IX/IY to BC/DE via PUSH/POP.
          RC = &Z80::GR16RegClass;
        } else {
          // F, FLAGS, shadow registers — use LLT type.
          LLT Ty = MRI.getType(DstReg);
          RC = (Ty.isValid() && Ty.getSizeInBits() <= 8) ? &Z80::GR8RegClass
                                                         : &Z80::GR16RegClass;
        }
        // Try to constrain; if it fails, the register may have incompatible
        // constraints from multiple uses. We'll handle this during register
        // allocation with copyPhysReg.
        RBI.constrainGenericRegister(DstReg, *RC, MRI);
        // Don't fail here - let register allocator handle it
      }
      // Both virtual: propagate register class from whichever side has one,
      // or assign a default class based on the LLT type.
      // NOTE: Cross-size COPYs (s16→s8, s8→s16) should have been converted
      // to G_TRUNC/G_ANYEXT by the post-legalization combiner. If any remain,
      // they will fail here — that's intentional to surface the bug early.
      else if (DstReg.isVirtual() && SrcReg.isVirtual()) {
        const TargetRegisterClass *DstRC = MRI.getRegClassOrNull(DstReg);
        const TargetRegisterClass *SrcRC = MRI.getRegClassOrNull(SrcReg);
        if (DstRC && !SrcRC)
          RBI.constrainGenericRegister(SrcReg, *DstRC, MRI);
        else if (SrcRC && !DstRC)
          RBI.constrainGenericRegister(DstReg, *SrcRC, MRI);
        else if (!DstRC && !SrcRC) {
          // Neither has a class — assign based on LLT type
          LLT Ty = MRI.getType(DstReg);
          if (!Ty.isValid())
            Ty = MRI.getType(SrcReg);
          if (Ty.isValid()) {
            const TargetRegisterClass *RC = Ty.getSizeInBits() <= 8
                                                ? &Z80::GR8RegClass
                                                : &Z80::GR16RegClass;
            RBI.constrainGenericRegister(DstReg, *RC, MRI);
            RBI.constrainGenericRegister(SrcReg, *RC, MRI);
          }
        }
        return true;
      }
      // If source is virtual and destination is physical, check for conflicts
      else if (SrcReg.isVirtual() && DstReg.isPhysical()) {
        const TargetRegisterClass *DstRC;
        if (DstReg == Z80::SP) {
          DstRC = &Z80::HLIRegClass;
        } else if (Z80::GR16RegClass.contains(DstReg)) {
          DstRC = &Z80::GR16RegClass;
        } else if (Z80::GR8RegClass.contains(DstReg)) {
          DstRC = &Z80::GR8RegClass;
        } else if (Z80::IR16RegClass.contains(DstReg)) {
          // copyPhysReg handles BC/DE to IX/IY via PUSH/POP.
          DstRC = &Z80::GR16RegClass;
        } else {
          LLT Ty = MRI.getType(SrcReg);
          DstRC = (Ty.isValid() && Ty.getSizeInBits() <= 8)
                      ? &Z80::GR8RegClass
                      : &Z80::GR16RegClass;
        }
        const TargetRegisterClass *SrcRC = MRI.getRegClassOrNull(SrcReg);

        // If source already has a conflicting register class, we need to
        // emit explicit copy instructions via PUSH/POP
        if (SrcRC && DstRC) {
          // Check if the intersection is empty or problematic
          const TargetRegisterClass *Common =
              TRI.getCommonSubClass(SrcRC, DstRC);

          if (!Common || Common->getNumRegs() == 0) {
            // Incompatible classes - emit PUSH/POP sequence for 16-bit regs
            LLT Ty = MRI.getType(SrcReg);
            if (Ty.isValid() && Ty.getSizeInBits() == 16) {
              // Get push opcode for source's physical register
              // First, we need to get the actual physical reg that will be used
              // For now, emit a generic sequence using BC as intermediate
              // PUSH src_class; POP dst_class
              // But we don't know the physical source yet...

              // Alternative: Don't constrain here, let the register allocator
              // insert the copy via copyPhysReg which handles PUSH/POP
              // Just mark as needing special handling
              return true;
            }
          }
        }

        // Try to constrain
        if (!RBI.constrainGenericRegister(SrcReg, *DstRC, MRI)) {
          // If constraining fails, still return true and let register
          // allocator handle it via spill/reload or copyPhysReg
          return true;
        }
      }
      return true;
    }

    // For target instructions, just verify they're okay
    constrainSelectedInstRegOperands(MI, TII, TRI,
                                     *MF.getSubtarget().getRegBankInfo());
    return true;
  }

  // Dead code elimination for folded generic instructions.
  // When IX-indexed load patterns are folded, the address computation
  // (G_PTR_ADD, G_CONSTANT) becomes dead. Clean it up here.
  // For multi-def instructions (e.g. G_UNMERGE_VALUES), ALL defs must be
  // dead before we can safely delete the instruction.
  if (MI.getNumDefs() > 0) {
    bool AllDefsDead = true;
    for (unsigned I = 0, E = MI.getNumDefs(); I < E; ++I) {
      Register DefReg = MI.getOperand(I).getReg();
      if (!DefReg.isVirtual() || !MRI.use_nodbg_empty(DefReg)) {
        AllDefsDead = false;
        break;
      }
    }
    if (AllDefsDead && !MI.mayLoadOrStore() && !MI.hasUnmodeledSideEffects()) {
      MI.eraseFromParent();
      return true;
    }
  }

  // Helper: check if a register is defined by a single-use G_LOAD from a
  // frame index (G_FRAME_INDEX or G_PTR_ADD(G_FRAME_INDEX, G_CONSTANT)).
  // Returns {FI, Offset, LoadMI} or {-1, 0, nullptr} if not foldable.
  struct FILoadInfo {
    int FI;
    int64_t Offset;
    MachineInstr *LoadMI;
  };
  auto getFILoad = [&](Register Reg) -> FILoadInfo {
    if (!Reg.isVirtual() || !MRI.hasOneNonDBGUse(Reg))
      return {-1, 0, nullptr};
    MachineInstr *LoadMI = MRI.getVRegDef(Reg);
    if (!LoadMI || LoadMI->getOpcode() != TargetOpcode::G_LOAD)
      return {-1, 0, nullptr};
    if (LoadMI->getParent() != &MBB)
      return {-1, 0, nullptr};
    // Check address operand: G_FRAME_INDEX or G_PTR_ADD(G_FRAME_INDEX, const)
    Register AddrReg = LoadMI->getOperand(1).getReg();
    MachineInstr *AddrDef = MRI.getVRegDef(AddrReg);
    if (!AddrDef)
      return {-1, 0, nullptr};
    if (AddrDef->getOpcode() == TargetOpcode::G_FRAME_INDEX)
      return {AddrDef->getOperand(1).getIndex(), 0, LoadMI};
    if (AddrDef->getOpcode() == TargetOpcode::G_PTR_ADD) {
      MachineInstr *BaseDef = MRI.getVRegDef(AddrDef->getOperand(1).getReg());
      MachineInstr *OffDef = MRI.getVRegDef(AddrDef->getOperand(2).getReg());
      if (BaseDef && BaseDef->getOpcode() == TargetOpcode::G_FRAME_INDEX &&
          OffDef && OffDef->getOpcode() == TargetOpcode::G_CONSTANT)
        return {BaseDef->getOperand(1).getIndex(),
                OffDef->getOperand(1).getCImm()->getSExtValue(), LoadMI};
    }
    return {-1, 0, nullptr};
  };

  // Helper: move LIFETIME_END for a given FI from between LoadMI and MI
  // to after InsertPt. This prevents StackColoring from merging the slot
  // before the folded read occurs.
  auto moveLifetimeEnd = [&](MachineInstr *LoadMI, MachineInstr &UseMI,
                             MachineBasicBlock::iterator InsertPt, int FI) {
    for (auto SIt = std::next(MachineBasicBlock::iterator(LoadMI));
         SIt != MachineBasicBlock::iterator(UseMI);) {
      MachineInstr &Cur = *SIt++;
      if (Cur.getOpcode() != TargetOpcode::LIFETIME_END)
        continue;
      for (const MachineOperand &MO : Cur.operands()) {
        if (MO.isFI() && MO.getIndex() == FI) {
          MBB.splice(InsertPt, &MBB, &Cur);
          break;
        }
      }
    }
  };

  // Helper: try to fold a FI load into ADD_HL_FI or SUB_HL_FI.
  // HLSrcReg is copied into HL (the accumulator side of the 16-bit op).
  // FoldReg is the candidate whose defining G_LOAD from a frame index
  // will be folded into the pseudo.  Returns true if the fold was emitted.
  auto tryFIFold = [&](Register HLSrcReg, Register FoldReg, Register DstReg,
                       unsigned FoldOpc) -> bool {
    FILoadInfo FIInfo = getFILoad(FoldReg);
    if (FIInfo.FI < 0)
      return false;
    if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(HLSrcReg, Z80::GR16RegClass, MRI))
      return false;
    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::HL)
        .addReg(HLSrcReg);
    auto MIB = BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(FoldOpc));
    MIB.addFrameIndex(FIInfo.FI);
    if (FIInfo.Offset)
      MIB.addImm(FIInfo.Offset);
    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
        .addReg(Z80::HL);
    moveLifetimeEnd(FIInfo.LoadMI, MI,
                    std::next(MachineBasicBlock::iterator(*MIB.getInstr())),
                    FIInfo.FI);
    FIInfo.LoadMI->eraseFromParent();
    MI.eraseFromParent();
    return true;
  };

  // Handle generic opcodes
  switch (Opcode) {
  default:
    return false;

  case TargetOpcode::G_FREEZE:
  case TargetOpcode::G_INTTOPTR:
  case TargetOpcode::G_PTRTOINT:
  case TargetOpcode::G_BITCAST: {
    // These are no-ops at the machine level. Lower to a COPY.
    Register DstReg = MI.getOperand(0).getReg();
    Register SrcReg = MI.getOperand(1).getReg();
    const LLT DstTy = MRI.getType(DstReg);
    const LLT SrcTy = MRI.getType(SrcReg);
    const TargetRegisterClass *DstRC =
        DstTy.getSizeInBits() <= 8 ? &Z80::GR8RegClass : &Z80::GR16RegClass;
    const TargetRegisterClass *SrcRC =
        SrcTy.getSizeInBits() <= 8 ? &Z80::GR8RegClass : &Z80::GR16RegClass;
    if (!RBI.constrainGenericRegister(DstReg, *DstRC, MRI) ||
        !RBI.constrainGenericRegister(SrcReg, *SrcRC, MRI))
      return false;
    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
        .addReg(SrcReg);
    MI.eraseFromParent();
    return true;
  }

  case TargetOpcode::G_SEXT_INREG: {
    // Sign extend in register: sext_inreg i16, 8
    // Uses SEXT_GR8_GR16 pseudo: LD A,src_lo; LD dst_lo,A; RLCA; SBC A,A; LD
    // dst_hi,A
    Register DstReg = MI.getOperand(0).getReg();
    Register SrcReg = MI.getOperand(1).getReg();
    int64_t Width = MI.getOperand(2).getImm();
    const LLT DstTy = MRI.getType(DstReg);

    if (DstTy.getSizeInBits() == 16 && Width == 8) {
      // Extract low byte, then sign-extend to 16-bit
      Register LowReg = MRI.createVirtualRegister(&Z80::GR8RegClass);
      if (!RBI.constrainGenericRegister(SrcReg, Z80::GR16RegClass, MRI) ||
          !RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI))
        return false;
      // Extract low byte from source
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), LowReg)
          .addReg(SrcReg, RegState{}, Z80::sub_lo);
      // Sign-extend to 16-bit
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::SEXT_GR8_GR16), DstReg)
          .addReg(LowReg);
      MI.eraseFromParent();
      return true;
    }
    // Fallback: not handled, let legalizer lower to SHL+ASHR
    return false;
  }

  case TargetOpcode::G_CONSTANT: {
    // Materialize constant into register
    Register DstReg = MI.getOperand(0).getReg();
    const LLT DstTy = MRI.getType(DstReg);
    int64_t Val = MI.getOperand(1).getCImm()->getSExtValue();

    if (DstTy.getSizeInBits() <= 8) {
      // Constrain destination to 8-bit register class
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI))
        return false;
      // 8-bit constant: LD r,n (pseudo, expanded after RA)
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::LD_r8_n), DstReg)
          .addImm(Val & 0xFF);
    } else if (DstTy.getSizeInBits() <= 16) {
      // Constrain destination to 16-bit register class
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI))
        return false;
      // 16-bit constant: LD rr,nn (pseudo, expanded after RA)
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::LD_r16_nn), DstReg)
          .addImm(Val & 0xFFFF);
    } else {
      return false;
    }

    MI.eraseFromParent();
    return true;
  }

  case TargetOpcode::G_FRAME_INDEX: {
    // Materialize the address of a stack object into a register.
    // LEA_IX_FI carries the frame index and is resolved by
    // eliminateFrameIndex to compute IX + offset.
    Register DstReg = MI.getOperand(0).getReg();
    int FI = MI.getOperand(1).getIndex();

    if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI))
      return false;

    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::LEA_IX_FI), DstReg)
        .addFrameIndex(FI);

    MI.eraseFromParent();
    return true;
  }

  case TargetOpcode::G_GLOBAL_VALUE: {
    // Load address of global variable
    Register DstReg = MI.getOperand(0).getReg();
    const GlobalValue *GV = MI.getOperand(1).getGlobal();

    // Constrain destination to 16-bit register class
    if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI))
      return false;

    // Use LD_r16_nn pseudo with the global's address
    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::LD_r16_nn), DstReg)
        .addGlobalAddress(GV);
    MI.eraseFromParent();
    return true;
  }

  case TargetOpcode::G_BLOCK_ADDR: {
    // Load address of a basic block (for computed goto)
    Register DstReg = MI.getOperand(0).getReg();
    const BlockAddress *BA = MI.getOperand(1).getBlockAddress();

    if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI))
      return false;

    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::LD_r16_nn), DstReg)
        .addBlockAddress(BA);
    MI.eraseFromParent();
    return true;
  }

  case TargetOpcode::G_JUMP_TABLE: {
    // Materialize jump table base address into a register
    Register DstReg = MI.getOperand(0).getReg();
    unsigned JTI = MI.getOperand(1).getIndex();

    if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI))
      return false;

    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::LD_r16_nn), DstReg)
        .addJumpTableIndex(JTI);
    MI.eraseFromParent();
    return true;
  }

  case TargetOpcode::G_LOAD: {
    // Load from memory
    Register DstReg = MI.getOperand(0).getReg();
    Register AddrReg = MI.getOperand(1).getReg();
    const LLT DstTy = MRI.getType(DstReg);
    const DebugLoc &DL = MI.getDebugLoc();

    // Try IX-indexed addressing: match G_PTR_ADD(COPY $ix, G_CONSTANT d)
    // This produces LD r,(IX+d) instead of the multi-instruction HL-indirect
    // sequence, which is much more efficient for stack argument access.
    MachineInstr *AddrDef = MRI.getVRegDef(AddrReg);
    if (AddrDef && AddrDef->getOpcode() == TargetOpcode::G_PTR_ADD) {
      Register BaseReg = AddrDef->getOperand(1).getReg();
      Register OffsetReg = AddrDef->getOperand(2).getReg();
      MachineInstr *BaseDef = MRI.getVRegDef(BaseReg);
      MachineInstr *OffsetDef = MRI.getVRegDef(OffsetReg);

      bool IsIXBase = BaseDef && BaseDef->getOpcode() == TargetOpcode::COPY &&
                      BaseDef->getOperand(1).isReg() &&
                      BaseDef->getOperand(1).getReg() == Z80::IX;

      int64_t Disp = 0;
      bool IsConstOffset =
          OffsetDef && OffsetDef->getOpcode() == TargetOpcode::G_CONSTANT;
      if (IsConstOffset)
        Disp = OffsetDef->getOperand(1).getCImm()->getSExtValue();

      if (IsIXBase && IsConstOffset) {
        if (DstTy.getSizeInBits() <= 8 && Disp >= -128 && Disp <= 127) {
          // 8-bit IX-indexed load: LD A,(IX+d)
          if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI))
            return false;
          BuildMI(MBB, MI, DL, TII.get(Z80::LD_A_IXd))
              .addImm(Disp)
              .addReg(Z80::A, RegState::ImplicitDefine);
          BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg)
              .addReg(Z80::A);
          MI.eraseFromParent();
          return true;
        }
        if (DstTy.getSizeInBits() <= 16 && Disp >= -128 && Disp + 1 <= 127) {
          // 16-bit IX-indexed load.
          // Choose target register pair based on downstream usage:
          // if the only use is a COPY to a physical register pair (DE, BC),
          // load directly into that pair to help the RA coalesce the COPY.
          if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI))
            return false;

          Register TargetPair = Z80::HL;
          unsigned LdLoOpc = Z80::LD_L_IXd;
          unsigned LdHiOpc = Z80::LD_H_IXd;

          if (MRI.hasOneNonDBGUse(DstReg)) {
            MachineInstr &Use = *MRI.use_nodbg_begin(DstReg)->getParent();
            if (Use.getOpcode() == TargetOpcode::COPY &&
                Use.getOperand(0).getReg().isPhysical()) {
              Register PhysDst = Use.getOperand(0).getReg();
              if (PhysDst == Z80::DE) {
                TargetPair = Z80::DE;
                LdLoOpc = Z80::LD_E_IXd;
                LdHiOpc = Z80::LD_D_IXd;
              } else if (PhysDst == Z80::BC) {
                TargetPair = Z80::BC;
                LdLoOpc = Z80::LD_C_IXd;
                LdHiOpc = Z80::LD_B_IXd;
              }
            }
          }

          BuildMI(MBB, MI, DL, TII.get(LdLoOpc))
              .addImm(Disp)
              .addReg(TargetPair, RegState::ImplicitDefine);
          BuildMI(MBB, MI, DL, TII.get(LdHiOpc))
              .addImm(Disp + 1)
              .addReg(TargetPair, RegState::ImplicitDefine);
          BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg)
              .addReg(TargetPair);
          MI.eraseFromParent();
          return true;
        }
      }

      // G_PTR_ADD(G_FRAME_INDEX, G_CONSTANT) - frame-relative with extra offset
      // Used when multi-byte locals are narrowed (e.g., 32-bit stored as two
      // 16-bit halves: low half at FI, high half at FI+2).
      // Use RELOAD pseudos which properly declare HL/BC clobbers for large
      // offsets.
      bool IsFrameBase =
          BaseDef && BaseDef->getOpcode() == TargetOpcode::G_FRAME_INDEX;
      if (IsFrameBase && IsConstOffset) {
        int FI = BaseDef->getOperand(1).getIndex();

        if (DstTy.getSizeInBits() <= 8) {
          if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI))
            return false;
          BuildMI(MBB, MI, DL, TII.get(Z80::RELOAD_GR8), DstReg)
              .addFrameIndex(FI)
              .addImm(Disp);
          MI.eraseFromParent();
          return true;
        }
        if (DstTy.getSizeInBits() <= 16) {
          if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI))
            return false;
          BuildMI(MBB, MI, DL, TII.get(Z80::RELOAD_GR16), DstReg)
              .addFrameIndex(FI)
              .addImm(Disp);
          MI.eraseFromParent();
          return true;
        }
      }
    }

    // Try IX-indexed addressing from G_FRAME_INDEX (no extra offset)
    // Use RELOAD pseudos which properly declare HL/BC clobbers for large
    // offsets.
    if (AddrDef && AddrDef->getOpcode() == TargetOpcode::G_FRAME_INDEX) {
      int FI = AddrDef->getOperand(1).getIndex();

      if (DstTy.getSizeInBits() <= 8) {
        if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI))
          return false;
        BuildMI(MBB, MI, DL, TII.get(Z80::RELOAD_GR8), DstReg)
            .addFrameIndex(FI)
            .addImm(0);
        MI.eraseFromParent();
        return true;
      }
      if (DstTy.getSizeInBits() <= 16) {
        if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI))
          return false;
        BuildMI(MBB, MI, DL, TII.get(Z80::RELOAD_GR16), DstReg)
            .addFrameIndex(FI)
            .addImm(0);
        MI.eraseFromParent();
        return true;
      }
    }

    // Fallback: HL-indirect addressing
    if (DstTy.getSizeInBits() <= 8) {
      // 8-bit load via HL indirection
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI) ||
          !RBI.constrainGenericRegister(AddrReg, Z80::GR16RegClass, MRI))
        return false;

      BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::HL)
          .addReg(AddrReg);
      BuildMI(MBB, MI, DL, TII.get(Z80::LD_A_HLind));
      BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg).addReg(Z80::A);
      MI.eraseFromParent();
      return true;
    }

    if (DstTy.getSizeInBits() <= 16) {
      // 16-bit load: load low byte, then high byte
      // addr -> HL, load (HL) to E, inc HL, load (HL) to D
      // Result in DE, then copy to DstReg
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI) ||
          !RBI.constrainGenericRegister(AddrReg, Z80::GR16RegClass, MRI))
        return false;

      BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::HL)
          .addReg(AddrReg);
      // Load low byte
      BuildMI(MBB, MI, DL, TII.get(Z80::LD_E_HLind));
      // Increment address
      BuildMI(MBB, MI, DL, TII.get(Z80::INC_HL));
      // Load high byte
      BuildMI(MBB, MI, DL, TII.get(Z80::LD_D_HLind));
      // Copy result to destination
      BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg).addReg(Z80::DE);
      MI.eraseFromParent();
      return true;
    }
    return false;
  }

  case TargetOpcode::G_STORE: {
    // Store to memory
    Register SrcReg = MI.getOperand(0).getReg();
    Register AddrReg = MI.getOperand(1).getReg();
    const LLT SrcTy = MRI.getType(SrcReg);
    const DebugLoc &DL = MI.getDebugLoc();

    // Try IX-indexed addressing from G_FRAME_INDEX or
    // G_PTR_ADD(G_FRAME_INDEX, G_CONSTANT)
    {
      MachineInstr *AddrDef = MRI.getVRegDef(AddrReg);
      int FI = -1;
      int64_t ExtraOffset = 0;
      bool IsFrameAddr = false;

      if (AddrDef && AddrDef->getOpcode() == TargetOpcode::G_FRAME_INDEX) {
        FI = AddrDef->getOperand(1).getIndex();
        IsFrameAddr = true;
      } else if (AddrDef && AddrDef->getOpcode() == TargetOpcode::G_PTR_ADD) {
        Register BaseReg = AddrDef->getOperand(1).getReg();
        Register OffReg = AddrDef->getOperand(2).getReg();
        MachineInstr *BaseDef = MRI.getVRegDef(BaseReg);
        MachineInstr *OffDef = MRI.getVRegDef(OffReg);
        if (BaseDef && BaseDef->getOpcode() == TargetOpcode::G_FRAME_INDEX &&
            OffDef && OffDef->getOpcode() == TargetOpcode::G_CONSTANT) {
          FI = BaseDef->getOperand(1).getIndex();
          ExtraOffset = OffDef->getOperand(1).getCImm()->getSExtValue();
          IsFrameAddr = true;
        }
      }

      // Use SPILL pseudos which properly declare HL/BC clobbers for large
      // offsets.
      if (IsFrameAddr) {
        if (SrcTy.getSizeInBits() <= 8) {
          // Check for constant store → use LD (IX+d),n via SPILL_IMM8
          MachineInstr *SrcDef = MRI.getVRegDef(SrcReg);
          if (SrcDef && SrcDef->getOpcode() == TargetOpcode::G_CONSTANT) {
            int64_t Val = SrcDef->getOperand(1).getCImm()->getSExtValue();
            BuildMI(MBB, MI, DL, TII.get(Z80::SPILL_IMM8))
                .addImm(Val & 0xFF)
                .addFrameIndex(FI)
                .addImm(ExtraOffset);
            MI.eraseFromParent();
            return true;
          }
          if (!RBI.constrainGenericRegister(SrcReg, Z80::GR8RegClass, MRI))
            return false;
          BuildMI(MBB, MI, DL, TII.get(Z80::SPILL_GR8))
              .addReg(SrcReg)
              .addFrameIndex(FI)
              .addImm(ExtraOffset);
          MI.eraseFromParent();
          return true;
        }
        if (SrcTy.getSizeInBits() <= 16) {
          if (!RBI.constrainGenericRegister(SrcReg, Z80::GR16RegClass, MRI))
            return false;
          BuildMI(MBB, MI, DL, TII.get(Z80::SPILL_GR16))
              .addReg(SrcReg)
              .addFrameIndex(FI)
              .addImm(ExtraOffset);
          MI.eraseFromParent();
          return true;
        }
      }
    }

    if (SrcTy.getSizeInBits() <= 8) {
      // 8-bit store via HL indirection
      if (!RBI.constrainGenericRegister(SrcReg, Z80::GR8RegClass, MRI) ||
          !RBI.constrainGenericRegister(AddrReg, Z80::GR16RegClass, MRI))
        return false;

      BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::HL)
          .addReg(AddrReg);
      BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A).addReg(SrcReg);
      BuildMI(MBB, MI, DL, TII.get(Z80::LD_HLind_A));
      MI.eraseFromParent();
      return true;
    }

    if (SrcTy.getSizeInBits() <= 16) {
      // 16-bit store: store low byte, then high byte
      // Copy value to DE, addr to HL, store E to (HL), inc HL, store D to (HL)
      if (!RBI.constrainGenericRegister(SrcReg, Z80::GR16RegClass, MRI) ||
          !RBI.constrainGenericRegister(AddrReg, Z80::GR16RegClass, MRI))
        return false;

      // Copy value to DE (for E=low, D=high).
      // For undef sources, skip the COPY and mark the implicit sub-register
      // uses as undef directly — processImplicitDefs only propagates undef
      // to the first user instruction, missing subsequent sub-register uses.
      MachineInstr *SrcDef = MRI.getVRegDef(SrcReg);
      bool IsUndef = SrcDef &&
                     SrcDef->getOpcode() == TargetOpcode::G_IMPLICIT_DEF;

      if (!IsUndef)
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::DE)
            .addReg(SrcReg);
      // Copy address to HL
      BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::HL)
          .addReg(AddrReg);
      // Store low byte directly from E (no A intermediary)
      auto &StoreLo = *BuildMI(MBB, MI, DL, TII.get(Z80::LD_HLind_E));
      // Increment address
      BuildMI(MBB, MI, DL, TII.get(Z80::INC_HL));
      // Store high byte directly from D
      auto &StoreHi = *BuildMI(MBB, MI, DL, TII.get(Z80::LD_HLind_D));
      if (IsUndef) {
        StoreLo.findRegisterUseOperand(Z80::E, /*TRI=*/nullptr)->setIsUndef();
        StoreHi.findRegisterUseOperand(Z80::D, /*TRI=*/nullptr)->setIsUndef();
      }
      MI.eraseFromParent();
      return true;
    }
    return false;
  }

  case TargetOpcode::G_PTR_ADD: {
    // Pointer addition: base pointer + offset → pointer
    // Same as 16-bit ADD since pointers are 16-bit on Z80
    Register DstReg = MI.getOperand(0).getReg();
    Register BaseReg = MI.getOperand(1).getReg();
    Register OffReg = MI.getOperand(2).getReg();

    // Check for small constant offset: repeated INC16/DEC16 (1 byte each)
    // is smaller than LD rr,nn + ADD HL,rr (4 bytes) for |offset| <= 3.
    MachineInstr *OffDef = MRI.getVRegDef(OffReg);
    if (OffDef && OffDef->getOpcode() == TargetOpcode::G_CONSTANT) {
      int64_t OffVal = OffDef->getOperand(1).getCImm()->getSExtValue();
      if (OffVal != 0 && std::abs(OffVal) <= 3) {
        if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI) ||
            !RBI.constrainGenericRegister(BaseReg, Z80::GR16RegClass, MRI))
          return false;
        unsigned PseudoOpc = (OffVal > 0) ? Z80::INC16 : Z80::DEC16;
        int64_t Count = std::abs(OffVal);
        Register PrevReg = BaseReg;
        for (int64_t i = 0; i < Count; i++) {
          Register OutReg = (i == Count - 1)
                                ? DstReg
                                : MRI.createVirtualRegister(&Z80::GR16RegClass);
          BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(PseudoOpc), OutReg)
              .addReg(PrevReg);
          PrevReg = OutReg;
        }
        MI.eraseFromParent();
        return true;
      }
    }

    // Try fold: if OffReg is a single-use G_LOAD from frame index,
    // fold into ADD_HL_FI to avoid a GR16_BCDE register allocation.
    if (STI.hasZ80() && CachedFoldCount > 2 &&
        tryFIFold(BaseReg, OffReg, DstReg, Z80::ADD_HL_FI))
      return true;

    if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(BaseReg, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(OffReg, Z80::GR16_BCDERegClass, MRI))
      return false;
    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::HL)
        .addReg(BaseReg);
    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::ADD_HL_rr)).addReg(OffReg);
    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
        .addReg(Z80::HL);
    MI.eraseFromParent();
    return true;
  }

  case TargetOpcode::G_ADD: {
    // Addition
    Register DstReg = MI.getOperand(0).getReg();
    Register Src1Reg = MI.getOperand(1).getReg();
    Register Src2Reg = MI.getOperand(2).getReg();
    const LLT DstTy = MRI.getType(DstReg);

    if (DstTy.getSizeInBits() <= 8) {
      // Check for constant operand (G_ADD is commutative)
      auto getConst8 = [&](Register Reg) -> std::optional<int64_t> {
        MachineInstr *Def = MRI.getVRegDef(Reg);
        if (Def && Def->getOpcode() == TargetOpcode::G_CONSTANT)
          return Def->getOperand(1).getCImm()->getSExtValue();
        return std::nullopt;
      };
      auto ConstVal1 = getConst8(Src1Reg);
      auto ConstVal2 = getConst8(Src2Reg);

      // Normalize: put constant in Src2
      if (ConstVal1 && !ConstVal2) {
        std::swap(Src1Reg, Src2Reg);
        std::swap(ConstVal1, ConstVal2);
      }

      if (ConstVal2) {
        int64_t Val = *ConstVal2;
        if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI) ||
            !RBI.constrainGenericRegister(Src1Reg, Z80::GR8RegClass, MRI))
          return false;

        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::A)
            .addReg(Src1Reg);

        if (Val == 1)
          BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::INC_A));
        else if (Val == -1 || (Val & 0xFF) == 0xFF)
          BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::DEC_A));
        else
          BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::ADD_A_n))
              .addImm(Val & 0xFF);

        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Z80::A);
        MI.eraseFromParent();
        return true;
      }

      // Constrain registers
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI) ||
          !RBI.constrainGenericRegister(Src1Reg, Z80::GR8RegClass, MRI) ||
          !RBI.constrainGenericRegister(Src2Reg, Z80::GR8RegClass, MRI))
        return false;

      // 8-bit add: Copy src1 to A, ADD A,src2, copy A to dst
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::A)
          .addReg(Src1Reg);
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::ADD_A_r)).addReg(Src2Reg);
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
          .addReg(Z80::A);
      MI.eraseFromParent();
      return true;
    }

    if (DstTy.getSizeInBits() <= 16) {
      // Constrain registers
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI) ||
          !RBI.constrainGenericRegister(Src1Reg, Z80::GR16RegClass, MRI) ||
          !RBI.constrainGenericRegister(Src2Reg, Z80::GR16RegClass, MRI))
        return false;

      // 16-bit add
      // Check for constant +1/-1 to use INC16/DEC16 pseudo
      {
        MachineInstr *Def1 = MRI.getVRegDef(Src1Reg);
        MachineInstr *Def2 = MRI.getVRegDef(Src2Reg);
        // G_ADD is commutative, check either operand for constant
        auto getConstVal = [](MachineInstr *Def) -> std::optional<int64_t> {
          if (Def && Def->getOpcode() == TargetOpcode::G_CONSTANT)
            return Def->getOperand(1).getCImm()->getSExtValue();
          return std::nullopt;
        };
        auto ConstVal1 = getConstVal(Def1);
        auto ConstVal2 = getConstVal(Def2);
        // Prefer the non-constant as the source register
        // Check for small constants that can use repeated INC16/DEC16.
        // INC16 is 1 byte each, vs LD rr,nn (3 bytes) + ADD HL,rr (1 byte).
        // Worth it for |constant| <= 3.
        Register SrcReg;
        int64_t Imm = 0;
        bool HasConst = false;
        auto isSmallConst = [](std::optional<int64_t> V) -> bool {
          return V && *V != 0 && std::abs(*V) <= 3;
        };
        if (isSmallConst(ConstVal2)) {
          SrcReg = Src1Reg;
          Imm = *ConstVal2;
          HasConst = true;
        } else if (isSmallConst(ConstVal1)) {
          SrcReg = Src2Reg;
          Imm = *ConstVal1;
          HasConst = true;
        }
        if (HasConst) {
          unsigned PseudoOpc = (Imm > 0) ? Z80::INC16 : Z80::DEC16;
          int64_t Count = std::abs(Imm);
          Register PrevReg = SrcReg;
          for (int64_t i = 0; i < Count; i++) {
            Register OutReg =
                (i == Count - 1)
                    ? DstReg
                    : MRI.createVirtualRegister(&Z80::GR16RegClass);
            BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(PseudoOpc), OutReg)
                .addReg(PrevReg);
            PrevReg = OutReg;
          }
          MI.eraseFromParent();
          return true;
        }
      }

      if (Src1Reg == Src2Reg) {
        // Self-add: use ADD HL,HL (doubles the value)
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::HL)
            .addReg(Src1Reg);
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::ADD_HL_HL));
      } else {
        // G_ADD is commutative. Prefer putting the operand that came from HL
        // as Src1 (→HL) to avoid unnecessary register swaps.
        MachineInstr *Def1 = MRI.getVRegDef(Src1Reg);
        MachineInstr *Def2 = MRI.getVRegDef(Src2Reg);
        bool Src1FromHL = Def1 && Def1->getOpcode() == TargetOpcode::COPY &&
                          Def1->getOperand(1).getReg() == Z80::HL;
        bool Src2FromHL = Def2 && Def2->getOpcode() == TargetOpcode::COPY &&
                          Def2->getOperand(1).getReg() == Z80::HL;
        if (Src2FromHL && !Src1FromHL) {
          std::swap(Src1Reg, Src2Reg);
          std::swap(Def1, Def2);
        }

        // Try fold: if either operand is a single-use G_LOAD from frame
        // index, fold into ADD_HL_FI.  G_ADD is commutative, so try Src2
        // first (preferred: Src1 stays in HL after HL-hint swap), then Src1.
        if (STI.hasZ80() && CachedFoldCount > 2 &&
            (tryFIFold(Src1Reg, Src2Reg, DstReg, Z80::ADD_HL_FI) ||
             tryFIFold(Src2Reg, Src1Reg, DstReg, Z80::ADD_HL_FI)))
          return true;

        // Src2 → GR16_BCDE (regalloc chooses BC or DE), Src1 → HL
        if (!RBI.constrainGenericRegister(Src2Reg, Z80::GR16_BCDERegClass, MRI))
          return false;
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::HL)
            .addReg(Src1Reg);
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::ADD_HL_rr))
            .addReg(Src2Reg);
      }
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
          .addReg(Z80::HL);
      MI.eraseFromParent();
      return true;
    }
    return false;
  }

  case TargetOpcode::G_SUB: {
    // Subtraction
    Register DstReg = MI.getOperand(0).getReg();
    Register Src1Reg = MI.getOperand(1).getReg();
    Register Src2Reg = MI.getOperand(2).getReg();
    const LLT DstTy = MRI.getType(DstReg);

    if (DstTy.getSizeInBits() <= 8) {
      // Check for constant RHS
      MachineInstr *Def2 = MRI.getVRegDef(Src2Reg);
      if (Def2 && Def2->getOpcode() == TargetOpcode::G_CONSTANT) {
        int64_t Val = Def2->getOperand(1).getCImm()->getSExtValue();
        if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI) ||
            !RBI.constrainGenericRegister(Src1Reg, Z80::GR8RegClass, MRI))
          return false;

        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::A)
            .addReg(Src1Reg);

        if (Val == 1)
          BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::DEC_A));
        else if (Val == -1 || (Val & 0xFF) == 0xFF)
          BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::INC_A));
        else
          BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::SUB_n))
              .addImm(Val & 0xFF);

        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Z80::A);
        MI.eraseFromParent();
        return true;
      }

      // Constrain registers
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI) ||
          !RBI.constrainGenericRegister(Src1Reg, Z80::GR8RegClass, MRI) ||
          !RBI.constrainGenericRegister(Src2Reg, Z80::GR8RegClass, MRI))
        return false;

      // 8-bit sub: Copy src1 to A, SUB src2, copy A to dst
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::A)
          .addReg(Src1Reg);
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::SUB_r)).addReg(Src2Reg);
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
          .addReg(Z80::A);
      MI.eraseFromParent();
      return true;
    }

    if (DstTy.getSizeInBits() <= 16) {
      // Constrain registers
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI) ||
          !RBI.constrainGenericRegister(Src1Reg, Z80::GR16RegClass, MRI) ||
          !RBI.constrainGenericRegister(Src2Reg, Z80::GR16RegClass, MRI))
        return false;

      // Check for small constant to use repeated DEC16/INC16 pseudo.
      // SUB N → DEC16 × N, SUB -N → INC16 × N. Worth it for |N| <= 3.
      {
        MachineInstr *Src2Def = MRI.getVRegDef(Src2Reg);
        if (Src2Def && Src2Def->getOpcode() == TargetOpcode::G_CONSTANT) {
          int64_t Val = Src2Def->getOperand(1).getCImm()->getSExtValue();
          if (Val != 0 && std::abs(Val) <= 3) {
            unsigned PseudoOpc = (Val > 0) ? Z80::DEC16 : Z80::INC16;
            int64_t Count = std::abs(Val);
            Register PrevReg = Src1Reg;
            for (int64_t i = 0; i < Count; i++) {
              Register OutReg =
                  (i == Count - 1)
                      ? DstReg
                      : MRI.createVirtualRegister(&Z80::GR16RegClass);
              BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(PseudoOpc), OutReg)
                  .addReg(PrevReg);
              PrevReg = OutReg;
            }
            MI.eraseFromParent();
            return true;
          }
        }
      }

      // Try fold: if Src2 is a single-use G_LOAD from frame index,
      // fold into SUB_HL_FI.  SUB is not commutative, so only Src2.
      if (STI.hasZ80() && CachedFoldCount > 2 &&
          tryFIFold(Src1Reg, Src2Reg, DstReg, Z80::SUB_HL_FI))
        return true;

      // 16-bit sub
      if (!RBI.constrainGenericRegister(Src2Reg, Z80::GR16_BCDERegClass, MRI))
        return false;
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::HL)
          .addReg(Src1Reg);
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::SUB_HL_rr))
          .addReg(Src2Reg);
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
          .addReg(Z80::HL);
      MI.eraseFromParent();
      return true;
    }
    return false;
  }

  case TargetOpcode::G_AND: {
    Register DstReg = MI.getOperand(0).getReg();
    Register Src1Reg = MI.getOperand(1).getReg();
    Register Src2Reg = MI.getOperand(2).getReg();
    const LLT DstTy = MRI.getType(DstReg);

    if (DstTy.getSizeInBits() <= 8) {
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI) ||
          !RBI.constrainGenericRegister(Src1Reg, Z80::GR8RegClass, MRI) ||
          !RBI.constrainGenericRegister(Src2Reg, Z80::GR8RegClass, MRI))
        return false;

      // Identity fold: AND with 0xFF → COPY (for 8-bit)
      auto isAllOnesConst = [&](Register Reg) -> bool {
        MachineInstr *Def = MRI.getVRegDef(Reg);
        if (!Def)
          return false;
        if (Def->getOpcode() == TargetOpcode::G_CONSTANT)
          return Def->getOperand(1).getCImm()->isAllOnesValue();
        if (Def->getOpcode() == TargetOpcode::G_UNMERGE_VALUES) {
          unsigned NumDefs = Def->getNumOperands() - 1;
          Register SrcReg = Def->getOperand(NumDefs).getReg();
          MachineInstr *SrcDef = MRI.getVRegDef(SrcReg);
          if (!SrcDef || SrcDef->getOpcode() != TargetOpcode::G_CONSTANT)
            return false;
          uint64_t FullVal = SrcDef->getOperand(1).getCImm()->getZExtValue();
          unsigned EltBits = MRI.getType(Reg).getSizeInBits();
          uint64_t Mask = (1ULL << EltBits) - 1;
          for (unsigned I = 0; I < NumDefs; ++I) {
            if (Def->getOperand(I).getReg() == Reg)
              return ((FullVal >> (I * EltBits)) & Mask) == Mask;
          }
        }
        return false;
      };

      // Zero fold: AND with 0 → result is always 0
      auto isZeroConst8 = [&](Register Reg) -> bool {
        auto V = getConst8(Reg);
        return V && (*V & 0xFF) == 0;
      };

      if (isAllOnesConst(Src2Reg)) {
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Src1Reg);
      } else if (isAllOnesConst(Src1Reg)) {
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Src2Reg);
      } else if (isZeroConst8(Src1Reg) || isZeroConst8(Src2Reg)) {
        // AND with 0 → load immediate 0
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::LD_r8_n), DstReg)
            .addImm(0);
      } else {
        // Try immediate fold: AND with constant → AND_n
        // Commute: put constant in Src2
        if (getConst8(Src1Reg) && !getConst8(Src2Reg))
          std::swap(Src1Reg, Src2Reg);
        auto ImmVal = getConst8(Src2Reg);

        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::A)
            .addReg(Src1Reg);
        if (ImmVal)
          BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::AND_n))
              .addImm(*ImmVal & 0xFF);
        else
          BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::AND_r))
              .addReg(Src2Reg);
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Z80::A);
      }
      MI.eraseFromParent();
      return true;
    }
    return false;
  }

  case TargetOpcode::G_OR:
  case TargetOpcode::G_XOR: {
    Register DstReg = MI.getOperand(0).getReg();
    Register Src1Reg = MI.getOperand(1).getReg();
    Register Src2Reg = MI.getOperand(2).getReg();
    const LLT DstTy = MRI.getType(DstReg);

    if (DstTy.getSizeInBits() <= 8) {
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI) ||
          !RBI.constrainGenericRegister(Src1Reg, Z80::GR8RegClass, MRI) ||
          !RBI.constrainGenericRegister(Src2Reg, Z80::GR8RegClass, MRI))
        return false;

      // Identity fold: OR/XOR with 0 → COPY
      auto isZeroConst = [&](Register Reg) -> bool {
        MachineInstr *Def = MRI.getVRegDef(Reg);
        if (!Def)
          return false;
        if (Def->getOpcode() == TargetOpcode::G_CONSTANT)
          return Def->getOperand(1).getCImm()->isZero();
        if (Def->getOpcode() == TargetOpcode::G_UNMERGE_VALUES) {
          unsigned NumDefs = Def->getNumOperands() - 1;
          Register SrcReg = Def->getOperand(NumDefs).getReg();
          MachineInstr *SrcDef = MRI.getVRegDef(SrcReg);
          if (!SrcDef || SrcDef->getOpcode() != TargetOpcode::G_CONSTANT)
            return false;
          uint64_t FullVal = SrcDef->getOperand(1).getCImm()->getZExtValue();
          unsigned EltBits = MRI.getType(Reg).getSizeInBits();
          for (unsigned I = 0; I < NumDefs; ++I) {
            if (Def->getOperand(I).getReg() == Reg) {
              return ((FullVal >> (I * EltBits)) & ((1ULL << EltBits) - 1)) ==
                     0;
            }
          }
        }
        return false;
      };

      if (isZeroConst(Src2Reg)) {
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Src1Reg);
      } else if (isZeroConst(Src1Reg)) {
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Src2Reg);
      } else {
        // Try immediate fold: OR/XOR with constant → OR_n/XOR_n
        // Commute: put constant in Src2
        if (getConst8(Src1Reg) && !getConst8(Src2Reg))
          std::swap(Src1Reg, Src2Reg);
        auto ImmVal = getConst8(Src2Reg);

        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::A)
            .addReg(Src1Reg);
        if (ImmVal) {
          unsigned ImmOpc =
              (Opcode == TargetOpcode::G_OR) ? Z80::OR_n : Z80::XOR_n;
          BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(ImmOpc))
              .addImm(*ImmVal & 0xFF);
        } else {
          unsigned AluOpc =
              (Opcode == TargetOpcode::G_OR) ? Z80::OR_r : Z80::XOR_r;
          BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(AluOpc)).addReg(Src2Reg);
        }
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Z80::A);
      }
      MI.eraseFromParent();
      return true;
    }
    return false;
  }

  case TargetOpcode::G_SHL: {
    // Shift left - handles constant shift amounts by unrolling
    Register DstReg = MI.getOperand(0).getReg();
    Register SrcReg = MI.getOperand(1).getReg();
    Register ShiftAmtReg = MI.getOperand(2).getReg();
    const LLT DstTy = MRI.getType(DstReg);
    const DebugLoc &DL = MI.getDebugLoc();

    // Check for constant shift amount
    MachineInstr *ShiftAmtDef = MRI.getVRegDef(ShiftAmtReg);
    int64_t ShiftAmt = -1;
    if (ShiftAmtDef && ShiftAmtDef->getOpcode() == TargetOpcode::G_CONSTANT)
      ShiftAmt = ShiftAmtDef->getOperand(1).getCImm()->getZExtValue();

    if (DstTy.getSizeInBits() <= 8) {
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI) ||
          !RBI.constrainGenericRegister(SrcReg, Z80::GR8RegClass, MRI) ||
          !RBI.constrainGenericRegister(ShiftAmtReg, Z80::GR8RegClass, MRI))
        return false;

      if (ShiftAmt == 0) {
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg)
            .addReg(SrcReg);
      } else if (ShiftAmt >= 8) {
        // Shift >= type size: result is 0
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A)
            .addReg(SrcReg);
        BuildMI(MBB, MI, DL, TII.get(Z80::XOR_A));
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Z80::A);
      } else if (ShiftAmt > 0) {
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A)
            .addReg(SrcReg);
        for (int64_t i = 0; i < ShiftAmt; i++)
          BuildMI(MBB, MI, DL, TII.get(Z80::ADD_A_A));
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Z80::A);
      } else {
        // Variable shift: use DJNZ loop pseudo
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A)
            .addReg(SrcReg);
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::B)
            .addReg(ShiftAmtReg);
        BuildMI(MBB, MI, DL, TII.get(Z80::SHL8_VAR));
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Z80::A);
      }
      MI.eraseFromParent();
      return true;
    }

    if (DstTy.getSizeInBits() <= 16) {
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI) ||
          !RBI.constrainGenericRegister(SrcReg, Z80::GR16RegClass, MRI) ||
          !RBI.constrainGenericRegister(ShiftAmtReg, Z80::GR8RegClass, MRI))
        return false;

      if (ShiftAmt == 0) {
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg)
            .addReg(SrcReg);
      } else if (ShiftAmt >= 16) {
        BuildMI(MBB, MI, DL, TII.get(Z80::LD_r16_nn), DstReg).addImm(0);
      } else if (ShiftAmt >= 13) {
        // SHL by 13-15: rotate right + mask is faster than byte-move + shift
        // E.g. SHL 15: RRCA puts bit 0 at bit 7, AND 0x80 keeps it
        Register LoByte = MRI.createVirtualRegister(&Z80::GR8RegClass);
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), LoByte)
            .addReg(SrcReg, RegState{}, Z80::sub_lo);
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A)
            .addReg(LoByte);
        for (int64_t i = 0; i < 16 - ShiftAmt; i++)
          BuildMI(MBB, MI, DL, TII.get(Z80::RRCA));
        BuildMI(MBB, MI, DL, TII.get(Z80::AND_n))
            .addImm((0xFF << (ShiftAmt - 8)) & 0xFF);
        BuildMI(MBB, MI, DL, TII.get(Z80::LD_H_A));
        BuildMI(MBB, MI, DL, TII.get(Z80::LD_L_n)).addImm(0);
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Z80::HL);
      } else if (ShiftAmt >= 8) {
        // SHL by 8-12: move low byte to high, clear low, then shift remainder
        // Optimize: if source is G_ZEXT from i8, load the 8-bit value directly
        // into H instead of loading full 16-bit and doing LD H,L.
        MachineInstr *SrcDef = MRI.getVRegDef(SrcReg);
        if (SrcDef && SrcDef->getOpcode() == TargetOpcode::G_ZEXT) {
          Register ZextSrc = SrcDef->getOperand(1).getReg();
          if (!RBI.constrainGenericRegister(ZextSrc, Z80::GR8RegClass, MRI))
            return false;
          BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A)
              .addReg(ZextSrc);
          BuildMI(MBB, MI, DL, TII.get(Z80::LD_H_A));
        } else {
          // Extract low byte directly to H, avoiding dead load of high byte
          Register LoByte = MRI.createVirtualRegister(&Z80::GR8RegClass);
          BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), LoByte)
              .addReg(SrcReg, RegState{}, Z80::sub_lo);
          BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::H)
              .addReg(LoByte);
        }
        BuildMI(MBB, MI, DL, TII.get(Z80::LD_L_n)).addImm(0);
        for (int64_t i = 8; i < ShiftAmt; i++)
          BuildMI(MBB, MI, DL, TII.get(Z80::ADD_HL_HL));
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Z80::HL);
      } else if (ShiftAmt > 0) {
        // 16-bit: Use ADD HL,HL for each shift by 1
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::HL)
            .addReg(SrcReg);
        for (int64_t i = 0; i < ShiftAmt; i++)
          BuildMI(MBB, MI, DL, TII.get(Z80::ADD_HL_HL));
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Z80::HL);
      } else {
        // Variable shift: use DJNZ loop pseudo
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::HL)
            .addReg(SrcReg);
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::B)
            .addReg(ShiftAmtReg);
        BuildMI(MBB, MI, DL, TII.get(Z80::SHL16_VAR));
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Z80::HL);
      }
      MI.eraseFromParent();
      return true;
    }
    return false;
  }

  case TargetOpcode::G_LSHR: {
    // Logical shift right - handles constant shift amounts by unrolling
    Register DstReg = MI.getOperand(0).getReg();
    Register SrcReg = MI.getOperand(1).getReg();
    Register ShiftAmtReg = MI.getOperand(2).getReg();
    const LLT DstTy = MRI.getType(DstReg);
    const DebugLoc &DL = MI.getDebugLoc();

    MachineInstr *ShiftAmtDef = MRI.getVRegDef(ShiftAmtReg);
    int64_t ShiftAmt = -1;
    if (ShiftAmtDef && ShiftAmtDef->getOpcode() == TargetOpcode::G_CONSTANT)
      ShiftAmt = ShiftAmtDef->getOperand(1).getCImm()->getZExtValue();

    if (DstTy.getSizeInBits() <= 8) {
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI) ||
          !RBI.constrainGenericRegister(SrcReg, Z80::GR8RegClass, MRI) ||
          !RBI.constrainGenericRegister(ShiftAmtReg, Z80::GR8RegClass, MRI))
        return false;

      if (ShiftAmt == 0) {
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg)
            .addReg(SrcReg);
      } else if (ShiftAmt >= 8) {
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A)
            .addReg(SrcReg);
        BuildMI(MBB, MI, DL, TII.get(Z80::XOR_A));
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Z80::A);
      } else if (ShiftAmt == 7) {
        // LSHR by 7: RLCA rotates bit7→bit0, AND 1 isolates it
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A)
            .addReg(SrcReg);
        BuildMI(MBB, MI, DL, TII.get(Z80::RLCA));
        BuildMI(MBB, MI, DL, TII.get(Z80::AND_n)).addImm(1);
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Z80::A);
      } else if (ShiftAmt >= 4 && STI.hasSM83()) {
        // SM83: SWAP A (nibble swap) + AND + remaining SRL
        // SWAP+AND = 4B vs 4×SRL = 8B per 4-shift base
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A)
            .addReg(SrcReg);
        BuildMI(MBB, MI, DL, TII.get(Z80::SWAP_A));
        BuildMI(MBB, MI, DL, TII.get(Z80::AND_n)).addImm(0x0F);
        for (int64_t i = 4; i < ShiftAmt; i++)
          BuildMI(MBB, MI, DL, TII.get(Z80::SRL_A));
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Z80::A);
      } else if (ShiftAmt > 0) {
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A)
            .addReg(SrcReg);
        for (int64_t i = 0; i < ShiftAmt; i++)
          BuildMI(MBB, MI, DL, TII.get(Z80::SRL_A));
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Z80::A);
      } else {
        // Variable shift: use DJNZ loop pseudo
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A)
            .addReg(SrcReg);
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::B)
            .addReg(ShiftAmtReg);
        BuildMI(MBB, MI, DL, TII.get(Z80::LSHR8_VAR));
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Z80::A);
      }
      MI.eraseFromParent();
      return true;
    }

    if (DstTy.getSizeInBits() <= 16) {
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI) ||
          !RBI.constrainGenericRegister(SrcReg, Z80::GR16RegClass, MRI) ||
          !RBI.constrainGenericRegister(ShiftAmtReg, Z80::GR8RegClass, MRI))
        return false;

      if (ShiftAmt == 0) {
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg)
            .addReg(SrcReg);
      } else if (ShiftAmt >= 16) {
        BuildMI(MBB, MI, DL, TII.get(Z80::LD_r16_nn), DstReg).addImm(0);
      } else if (ShiftAmt >= 13) {
        // LSHR by 13-15: rotate left + mask is faster than byte-move + shift
        // E.g. LSHR 15: RLCA puts bit 7 at bit 0, AND 0x01 keeps it
        Register HiByte = MRI.createVirtualRegister(&Z80::GR8RegClass);
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), HiByte)
            .addReg(SrcReg, RegState{}, Z80::sub_hi);
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A)
            .addReg(HiByte);
        for (int64_t i = 0; i < 16 - ShiftAmt; i++)
          BuildMI(MBB, MI, DL, TII.get(Z80::RLCA));
        BuildMI(MBB, MI, DL, TII.get(Z80::AND_n))
            .addImm(0xFF >> (ShiftAmt - 8));
        BuildMI(MBB, MI, DL, TII.get(Z80::LD_L_A));
        BuildMI(MBB, MI, DL, TII.get(Z80::LD_H_n)).addImm(0);
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Z80::HL);
      } else if (ShiftAmt >= 8) {
        // LSHR by 8-12: extract high byte to low, clear high, then shift
        // Extract high byte directly to L, avoiding dead load of low byte
        Register HiByte = MRI.createVirtualRegister(&Z80::GR8RegClass);
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), HiByte)
            .addReg(SrcReg, RegState{}, Z80::sub_hi);
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::L)
            .addReg(HiByte);
        BuildMI(MBB, MI, DL, TII.get(Z80::LD_H_n)).addImm(0);
        for (int64_t i = 8; i < ShiftAmt; i++)
          BuildMI(MBB, MI, DL, TII.get(Z80::SRL_L));
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Z80::HL);
      } else if (ShiftAmt > 0) {
        // 16-bit: chain LSHR16 pseudos (each shifts right by 1)
        Register Prev = SrcReg;
        for (int64_t i = 0; i < ShiftAmt; i++) {
          Register Next = (i == ShiftAmt - 1)
                              ? DstReg
                              : MRI.createVirtualRegister(&Z80::GR16RegClass);
          BuildMI(MBB, MI, DL, TII.get(Z80::LSHR16), Next).addReg(Prev);
          Prev = Next;
        }
      } else {
        // Variable shift: use DJNZ loop pseudo
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::HL)
            .addReg(SrcReg);
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::B)
            .addReg(ShiftAmtReg);
        BuildMI(MBB, MI, DL, TII.get(Z80::LSHR16_VAR));
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Z80::HL);
      }
      MI.eraseFromParent();
      return true;
    }
    return false;
  }

  case TargetOpcode::G_ASHR: {
    // Arithmetic shift right - handles constant shift amounts
    // Special case: shift by type_size-1 is sign extension (all sign bits)
    Register DstReg = MI.getOperand(0).getReg();
    Register SrcReg = MI.getOperand(1).getReg();
    Register ShiftAmtReg = MI.getOperand(2).getReg();
    const LLT DstTy = MRI.getType(DstReg);
    const DebugLoc &DL = MI.getDebugLoc();

    MachineInstr *ShiftAmtDef = MRI.getVRegDef(ShiftAmtReg);
    int64_t ShiftAmt = -1;
    if (ShiftAmtDef && ShiftAmtDef->getOpcode() == TargetOpcode::G_CONSTANT)
      ShiftAmt = ShiftAmtDef->getOperand(1).getCImm()->getZExtValue();

    if (DstTy.getSizeInBits() <= 8) {
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI) ||
          !RBI.constrainGenericRegister(SrcReg, Z80::GR8RegClass, MRI) ||
          !RBI.constrainGenericRegister(ShiftAmtReg, Z80::GR8RegClass, MRI))
        return false;

      if (ShiftAmt == 0) {
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg)
            .addReg(SrcReg);
      } else if (ShiftAmt >= 7) {
        // Sign extension: result is all sign bits (0x00 or 0xFF)
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A)
            .addReg(SrcReg);
        BuildMI(MBB, MI, DL, TII.get(Z80::ADD_A_A)); // carry = sign bit
        BuildMI(MBB, MI, DL, TII.get(Z80::SBC_A_A)); // A = 0xFF or 0x00
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Z80::A);
      } else if (ShiftAmt > 0) {
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A)
            .addReg(SrcReg);
        for (int64_t i = 0; i < ShiftAmt; i++)
          BuildMI(MBB, MI, DL, TII.get(Z80::SRA_A));
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Z80::A);
      } else {
        // Variable shift: use DJNZ loop pseudo
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A)
            .addReg(SrcReg);
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::B)
            .addReg(ShiftAmtReg);
        BuildMI(MBB, MI, DL, TII.get(Z80::ASHR8_VAR));
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Z80::A);
      }
      MI.eraseFromParent();
      return true;
    }

    if (DstTy.getSizeInBits() <= 16) {
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI) ||
          !RBI.constrainGenericRegister(SrcReg, Z80::GR16RegClass, MRI) ||
          !RBI.constrainGenericRegister(ShiftAmtReg, Z80::GR8RegClass, MRI))
        return false;

      // Check for SHL+ASHR pattern: sext_inreg optimization
      // SHL 8 + ASHR 8 on i16 = sign extend low byte → SEXT_GR8_GR16
      if (ShiftAmt == 8) {
        MachineInstr *SrcDef = MRI.getVRegDef(SrcReg);
        if (SrcDef && SrcDef->getOpcode() == TargetOpcode::G_SHL) {
          Register ShlAmtReg = SrcDef->getOperand(2).getReg();
          MachineInstr *ShlAmtDef = MRI.getVRegDef(ShlAmtReg);
          if (ShlAmtDef && ShlAmtDef->getOpcode() == TargetOpcode::G_CONSTANT &&
              ShlAmtDef->getOperand(1).getCImm()->getZExtValue() == 8) {
            // Matched SHL 8 + ASHR 8: use SEXT_GR8_GR16
            Register OrigReg = SrcDef->getOperand(1).getReg();
            Register LowReg = MRI.createVirtualRegister(&Z80::GR8RegClass);
            if (!RBI.constrainGenericRegister(OrigReg, Z80::GR16RegClass, MRI))
              return false;
            BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), LowReg)
                .addReg(OrigReg, RegState{}, Z80::sub_lo);
            BuildMI(MBB, MI, DL, TII.get(Z80::SEXT_GR8_GR16), DstReg)
                .addReg(LowReg);
            MI.eraseFromParent();
            return true;
          }
        }
      }

      if (ShiftAmt == 0) {
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg)
            .addReg(SrcReg);
      } else if (ShiftAmt >= 15) {
        // Sign extension: result = 0x0000 or 0xFFFF based on sign bit
        // Uses SEXT16 pseudo expanded post-RA to avoid clobbering src
        BuildMI(MBB, MI, DL, TII.get(Z80::SEXT16), DstReg).addReg(SrcReg);
      } else if (ShiftAmt >= 8) {
        // ASHR by 8+: LD L,H (byte shift), SRA L × (N-8) (remainder),
        // then sign-extend H: ADD A,A puts sign into carry, SBC A,A → 0/-1
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::HL)
            .addReg(SrcReg);
        BuildMI(MBB, MI, DL, TII.get(Z80::LD_L_H));
        for (int64_t i = 8; i < ShiftAmt; i++)
          BuildMI(MBB, MI, DL, TII.get(Z80::SRA_L));
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A)
            .addReg(Z80::L);
        BuildMI(MBB, MI, DL, TII.get(Z80::ADD_A_A));
        BuildMI(MBB, MI, DL, TII.get(Z80::SBC_A_A));
        BuildMI(MBB, MI, DL, TII.get(Z80::LD_H_A));
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Z80::HL);
      } else if (ShiftAmt > 0) {
        // Chain ASHR16 pseudos (each shifts right by 1)
        Register Prev = SrcReg;
        for (int64_t i = 0; i < ShiftAmt; i++) {
          Register Next = (i == ShiftAmt - 1)
                              ? DstReg
                              : MRI.createVirtualRegister(&Z80::GR16RegClass);
          BuildMI(MBB, MI, DL, TII.get(Z80::ASHR16), Next).addReg(Prev);
          Prev = Next;
        }
      } else {
        // Variable shift: use DJNZ loop pseudo
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::HL)
            .addReg(SrcReg);
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::B)
            .addReg(ShiftAmtReg);
        BuildMI(MBB, MI, DL, TII.get(Z80::ASHR16_VAR));
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Z80::HL);
      }
      MI.eraseFromParent();
      return true;
    }
    return false;
  }

  case TargetOpcode::G_ROTL:
  case TargetOpcode::G_ROTR: {
    // 8-bit rotation using native Z80 rotate instructions.
    // RLCA/RRCA are 1-byte instructions that rotate A by 1 bit.
    // For constant amounts, we unroll; for N>4 we rotate the other direction.
    // For variable amounts, we use a DJNZ loop pseudo.
    Register DstReg = MI.getOperand(0).getReg();
    Register SrcReg = MI.getOperand(1).getReg();
    Register AmtReg = MI.getOperand(2).getReg();
    const LLT DstTy = MRI.getType(DstReg);
    const DebugLoc &DL = MI.getDebugLoc();
    bool IsLeft = MI.getOpcode() == TargetOpcode::G_ROTL;

    if (DstTy.getSizeInBits() > 8)
      return false;

    if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI) ||
        !RBI.constrainGenericRegister(SrcReg, Z80::GR8RegClass, MRI) ||
        !RBI.constrainGenericRegister(AmtReg, Z80::GR8RegClass, MRI))
      return false;

    MachineInstr *AmtDef = MRI.getVRegDef(AmtReg);
    int64_t Amt = -1;
    if (AmtDef && AmtDef->getOpcode() == TargetOpcode::G_CONSTANT)
      Amt = AmtDef->getOperand(1).getCImm()->getZExtValue() & 7;

    if (Amt == 0) {
      BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg).addReg(SrcReg);
    } else if (Amt > 0) {
      // For amounts > 4, rotate the other direction (fewer instructions).
      // E.g. ROTL by 6 = ROTR by 2 (2 instructions instead of 6).
      unsigned Opc;
      int64_t Count;
      if (Amt <= 4) {
        Opc = IsLeft ? Z80::RLCA : Z80::RRCA;
        Count = Amt;
      } else {
        Opc = IsLeft ? Z80::RRCA : Z80::RLCA;
        Count = 8 - Amt;
      }
      BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A).addReg(SrcReg);
      for (int64_t i = 0; i < Count; i++)
        BuildMI(MBB, MI, DL, TII.get(Opc));
      BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg).addReg(Z80::A);
    } else {
      // Variable rotation: use DJNZ loop pseudo.
      BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A).addReg(SrcReg);
      BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::B).addReg(AmtReg);
      BuildMI(MBB, MI, DL, TII.get(IsLeft ? Z80::ROTL8_VAR : Z80::ROTR8_VAR));
      BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg).addReg(Z80::A);
    }
    MI.eraseFromParent();
    return true;
  }

  case TargetOpcode::G_ICMP: {
    // Integer comparison - produces branchless 0/1 result in s8.
    // Core technique: SBC A,A = -C (0xFF if carry, 0 otherwise), AND 1.

    Register DstReg = MI.getOperand(0).getReg();

    // If the result has no uses (fused into G_BRCOND), skip materialization.
    if (MRI.use_nodbg_empty(DstReg)) {
      MI.eraseFromParent();
      return true;
    }

    CmpInst::Predicate Pred =
        static_cast<CmpInst::Predicate>(MI.getOperand(1).getPredicate());
    Register LHS = MI.getOperand(2).getReg();
    Register RHS = MI.getOperand(3).getReg();
    const LLT LHSTy = MRI.getType(LHS);

    if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI))
      return false;

    if (LHSTy.getSizeInBits() <= 8) {
      // 8-bit comparison using generalized ALU pseudos.
      // Check if RHS is a constant for immediate-form instructions.
      auto getRHSConst = [&](Register Reg) -> std::optional<int64_t> {
        MachineInstr *Def = MRI.getVRegDef(Reg);
        if (Def && Def->getOpcode() == TargetOpcode::G_CONSTANT)
          return Def->getOperand(1).getCImm()->getSExtValue();
        return std::nullopt;
      };

      // Helper: emit SUB_r or SUB_n depending on whether operand is constant
      auto emitSUB = [&](Register Reg) {
        auto C = getRHSConst(Reg);
        if (C)
          BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::SUB_n))
              .addImm(*C & 0xFF);
        else
          BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::SUB_r)).addReg(Reg);
      };
      // Helper: emit CP_r or CP_n depending on whether operand is constant
      auto emitCP = [&](Register Reg) {
        auto C = getRHSConst(Reg);
        if (C)
          BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::CP_n))
              .addImm(*C & 0xFF);
        else
          BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::CP_r)).addReg(Reg);
      };

      if (!RBI.constrainGenericRegister(LHS, Z80::GR8RegClass, MRI) ||
          !RBI.constrainGenericRegister(RHS, Z80::GR8RegClass, MRI))
        return false;

      switch (Pred) {
      case CmpInst::ICMP_EQ:
        // EQ: A = LHS - RHS; SUB 1 (sets C only if A was 0); SBC A,A; AND 1
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::A)
            .addReg(LHS);
        emitSUB(RHS);
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::SUB_n)).addImm(1);
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::SBC_A_A));
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::AND_n)).addImm(1);
        break;

      case CmpInst::ICMP_NE:
        // NE: A = LHS - RHS; ADD 0xFF (sets C if A was non-zero); SBC A,A; AND
        // 1
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::A)
            .addReg(LHS);
        emitSUB(RHS);
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::ADD_A_n)).addImm(0xFF);
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::SBC_A_A));
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::AND_n)).addImm(1);
        break;

      case CmpInst::ICMP_ULT:
        // ULT: CP sets C if A < operand
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::A)
            .addReg(LHS);
        emitCP(RHS);
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::SBC_A_A));
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::AND_n)).addImm(1);
        break;

      case CmpInst::ICMP_UGE:
        // UGE: inverse of ULT
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::A)
            .addReg(LHS);
        emitCP(RHS);
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::CCF));
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::SBC_A_A));
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::AND_n)).addImm(1);
        break;

      case CmpInst::ICMP_UGT:
        // UGT = ULT with swapped operands: A=RHS, CP LHS
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::A)
            .addReg(RHS);
        emitCP(LHS);
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::SBC_A_A));
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::AND_n)).addImm(1);
        break;

      case CmpInst::ICMP_ULE:
        // ULE = UGE with swapped operands: A=RHS, CP LHS, CCF
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::A)
            .addReg(RHS);
        emitCP(LHS);
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::CCF));
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::SBC_A_A));
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::AND_n)).addImm(1);
        break;

      case CmpInst::ICMP_SLT:
      case CmpInst::ICMP_SGE:
      case CmpInst::ICMP_SGT:
      case CmpInst::ICMP_SLE: {
        bool SwapOps = (Pred == CmpInst::ICMP_SGT || Pred == CmpInst::ICMP_SLE);
        bool InvertC = (Pred == CmpInst::ICMP_SGE || Pred == CmpInst::ICMP_SLE);
        Register CmpLHS = SwapOps ? RHS : LHS;
        Register CmpRHS = SwapOps ? LHS : RHS;
        // ModLHS = CmpLHS ^ 0x80
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::A)
            .addReg(CmpLHS);
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::XOR_n)).addImm(0x80);
        Register ModLHS = MRI.createVirtualRegister(&Z80::GR8RegClass);
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), ModLHS)
            .addReg(Z80::A);
        // ModRHS = CmpRHS ^ 0x80
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::A)
            .addReg(CmpRHS);
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::XOR_n)).addImm(0x80);
        Register ModRHS = MRI.createVirtualRegister(&Z80::GR8RegClass);
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), ModRHS)
            .addReg(Z80::A);
        // CP: ModLHS - ModRHS
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::A)
            .addReg(ModLHS);
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::CP_r)).addReg(ModRHS);
        if (InvertC)
          BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::CCF));
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::SBC_A_A));
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::AND_n)).addImm(1);
        break;
      }

      default:
        return false;
      }
    } else if (LHSTy.getSizeInBits() <= 16) {
      DebugLoc DL = MI.getDebugLoc();

      if (Pred == CmpInst::ICMP_EQ || Pred == CmpInst::ICMP_NE) {
        // EQ/NE: XOR_CMP pseudo avoids SBC HL,DE (which clobbers HL).
        // Expanded post-RA to byte-level XOR with known physical regs.
        if (!RBI.constrainGenericRegister(LHS, Z80::GR16RegClass, MRI) ||
            !RBI.constrainGenericRegister(RHS, Z80::GR16RegClass, MRI))
          return false;
        unsigned PseudoOpc =
            (Pred == CmpInst::ICMP_EQ) ? Z80::XOR_CMP_EQ16 : Z80::XOR_CMP_NE16;
        BuildMI(MBB, MI, DL, TII.get(PseudoOpc)).addReg(LHS).addReg(RHS);
      } else if (ICmpInst::isSigned(Pred)) {
        // Special case: SLT/SGE against constant 0 → sign bit test.
        // icmp slt X, 0 = bit 7 of high byte; icmp sge X, 0 = inverted.
        auto isConstZero = [&](Register R) -> bool {
          MachineInstr *Def = MRI.getVRegDef(R);
          if (!Def || Def->getOpcode() != TargetOpcode::G_CONSTANT)
            return false;
          return Def->getOperand(1).getCImm()->isZero();
        };
        bool IsSignTest = false;
        Register SignTestReg;
        bool InvertSign = false;
        if ((Pred == CmpInst::ICMP_SLT || Pred == CmpInst::ICMP_SGE) &&
            isConstZero(RHS)) {
          IsSignTest = true;
          SignTestReg = LHS;
          InvertSign = (Pred == CmpInst::ICMP_SGE);
        } else if ((Pred == CmpInst::ICMP_SGT || Pred == CmpInst::ICMP_SLE) &&
                   isConstZero(LHS)) {
          // 0 > X  ↔  X < 0 (SLT);  0 <= X  ↔  X >= 0 (SGE)
          IsSignTest = true;
          SignTestReg = RHS;
          InvertSign = (Pred == CmpInst::ICMP_SLE);
        }
        if (IsSignTest) {
          if (!RBI.constrainGenericRegister(SignTestReg, Z80::GR16RegClass,
                                            MRI))
            return false;
          // Extract high byte and test sign bit: RLCA shifts bit 7 into bit 0
          Register HiByte = MRI.createVirtualRegister(&Z80::GR8RegClass);
          BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), HiByte)
              .addReg(SignTestReg, RegState{}, Z80::sub_hi);
          BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A)
              .addReg(HiByte);
          BuildMI(MBB, MI, DL, TII.get(Z80::RLCA));
          BuildMI(MBB, MI, DL, TII.get(Z80::AND_n)).addImm(1);
          if (InvertSign) {
            // Flip bit 0: XOR 1
            BuildMI(MBB, MI, DL, TII.get(Z80::XOR_n)).addImm(1);
          }
        } else {
          // Signed predicates: use generalized emitSigned16BitCompare.
          // SGT/SLE swap operands; SGE/SLE invert result.
          bool SwapOps =
              (Pred == CmpInst::ICMP_SGT || Pred == CmpInst::ICMP_SLE);
          bool Invert =
              (Pred == CmpInst::ICMP_SGE || Pred == CmpInst::ICMP_SLE);
          Register CmpLHS = SwapOps ? RHS : LHS;
          Register CmpRHS = SwapOps ? LHS : RHS;
          if (!RBI.constrainGenericRegister(CmpLHS, Z80::GR16RegClass, MRI) ||
              !RBI.constrainGenericRegister(CmpRHS, Z80::GR16_BCDERegClass,
                                            MRI))
            return false;
          emitSigned16BitCompare(MBB, MI, CmpLHS, CmpRHS, MRI, Invert);
        }
      } else {
        // Unsigned predicates (ULT, UGT, UGE, ULE): use CMP16_FLAGS.
        // CMP16_FLAGS uses 8-bit SUB/SBC chain and doesn't clobber HL,
        // avoiding register spills that SUB_HL_rr would cause.
        switch (Pred) {
        case CmpInst::ICMP_UGT:
          Pred = CmpInst::ICMP_ULT;
          std::swap(LHS, RHS);
          break;
        case CmpInst::ICMP_ULE:
          Pred = CmpInst::ICMP_UGE;
          std::swap(LHS, RHS);
          break;
        default:
          break;
        }

        if (!RBI.constrainGenericRegister(LHS, Z80::GR16RegClass, MRI) ||
            !RBI.constrainGenericRegister(RHS, Z80::GR16RegClass, MRI))
          return false;
        BuildMI(MBB, MI, DL, TII.get(Z80::CMP16_FLAGS)).addReg(LHS).addReg(RHS);

        switch (Pred) {
        case CmpInst::ICMP_ULT:
          BuildMI(MBB, MI, DL, TII.get(Z80::SBC_A_A));
          BuildMI(MBB, MI, DL, TII.get(Z80::AND_n)).addImm(1);
          break;
        case CmpInst::ICMP_UGE:
          BuildMI(MBB, MI, DL, TII.get(Z80::CCF));
          BuildMI(MBB, MI, DL, TII.get(Z80::SBC_A_A));
          BuildMI(MBB, MI, DL, TII.get(Z80::AND_n)).addImm(1);
          break;
        default:
          return false;
        }
      }
    } else {
      return false;
    }

    // Copy result from A to destination
    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
        .addReg(Z80::A);

    MI.eraseFromParent();
    return true;
  }

  case Z80::G_Z80_CMP_BR_EQ:
  case Z80::G_Z80_CMP_BR_NE:
  case Z80::G_Z80_CMP_BR_ULT:
  case Z80::G_Z80_CMP_BR_UGE: {
    // Fused compare-and-branch
    Register LHS = MI.getOperand(0).getReg();
    Register RHS = MI.getOperand(1).getReg();
    MachineBasicBlock *TargetBB = MI.getOperand(2).getMBB();
    const LLT LHSTy = MRI.getType(LHS);

    if (LHSTy.getSizeInBits() <= 8) {
      // 8-bit comparison: CP r (compare A with operand)
      if (!RBI.constrainGenericRegister(LHS, Z80::GR8RegClass, MRI) ||
          !RBI.constrainGenericRegister(RHS, Z80::GR8RegClass, MRI))
        return false;

      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::A)
          .addReg(LHS);
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::CP_r)).addReg(RHS);

      // Select the right conditional jump based on opcode
      unsigned JumpOpc;
      switch (Opcode) {
      case Z80::G_Z80_CMP_BR_EQ:
        JumpOpc = Z80::JP_Z_nn; // Jump if Zero (equal)
        break;
      case Z80::G_Z80_CMP_BR_NE:
        JumpOpc = Z80::JP_NZ_nn; // Jump if Not Zero (not equal)
        break;
      case Z80::G_Z80_CMP_BR_ULT:
        JumpOpc = Z80::JP_C_nn; // Jump if Carry (unsigned less than)
        break;
      case Z80::G_Z80_CMP_BR_UGE:
        JumpOpc = Z80::JP_NC_nn; // Jump if No Carry (unsigned greater or equal)
        break;
      default:
        return false;
      }

      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(JumpOpc)).addMBB(TargetBB);

      MI.eraseFromParent();
      return true;
    }

    if (LHSTy.getSizeInBits() <= 16) {
      // 16-bit fused compare-and-branch.
      // EQ/NE: Z80 uses SUB_HL_rr (SBC HL,rr sets Z correctly).
      //        SM83 uses SM83_CMP_Z16 (XOR+OR sets Z correctly).
      // ULT/UGE: use CMP16_FLAGS (8-bit SUB/SBC chain, carry flag).
      const auto &STI = MF.getSubtarget<Z80Subtarget>();
      if (Opcode == Z80::G_Z80_CMP_BR_EQ || Opcode == Z80::G_Z80_CMP_BR_NE) {
        if (STI.hasSM83()) {
          // SM83: XOR-based comparison sets Z flag correctly for 16-bit EQ/NE.
          if (!RBI.constrainGenericRegister(LHS, Z80::GR16RegClass, MRI) ||
              !RBI.constrainGenericRegister(RHS, Z80::GR16RegClass, MRI))
            return false;
          BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::SM83_CMP_Z16))
              .addReg(LHS)
              .addReg(RHS);
        } else {
          // Z80: AND A; SBC HL,rr sets Z flag correctly for 16-bit EQ/NE.
          if (!RBI.constrainGenericRegister(LHS, Z80::GR16RegClass, MRI) ||
              !RBI.constrainGenericRegister(RHS, Z80::GR16_BCDERegClass, MRI))
            return false;
          BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY),
                  Z80::HL)
              .addReg(LHS);
          BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::SUB_HL_rr))
              .addReg(RHS);
        }
      } else {
        if (!RBI.constrainGenericRegister(LHS, Z80::GR16RegClass, MRI) ||
            !RBI.constrainGenericRegister(RHS, Z80::GR16RegClass, MRI))
          return false;
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::CMP16_FLAGS))
            .addReg(LHS)
            .addReg(RHS);
      }

      unsigned JumpOpc;
      switch (Opcode) {
      case Z80::G_Z80_CMP_BR_EQ:
        JumpOpc = Z80::JP_Z_nn;
        break;
      case Z80::G_Z80_CMP_BR_NE:
        JumpOpc = Z80::JP_NZ_nn;
        break;
      case Z80::G_Z80_CMP_BR_ULT:
        JumpOpc = Z80::JP_C_nn;
        break;
      case Z80::G_Z80_CMP_BR_UGE:
        JumpOpc = Z80::JP_NC_nn;
        break;
      default:
        return false;
      }

      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(JumpOpc)).addMBB(TargetBB);

      MI.eraseFromParent();
      return true;
    }
    return false;
  }

  case Z80::G_Z80_ICMP32: {
    // 32-bit comparison via chained 8-bit SUB/SBC.
    // Operands: dst(i8), pred(imm), lhs_lo(i16), lhs_hi(i16),
    //           rhs_lo(i16), rhs_hi(i16)
    Register DstReg = MI.getOperand(0).getReg();
    auto Pred = static_cast<CmpInst::Predicate>(MI.getOperand(1).getImm());
    Register LhsLo = MI.getOperand(2).getReg();
    Register LhsHi = MI.getOperand(3).getReg();
    Register RhsLo = MI.getOperand(4).getReg();
    Register RhsHi = MI.getOperand(5).getReg();
    const DebugLoc &DL = MI.getDebugLoc();

    CmpInst::Predicate NormPred;
    if (!emit32CompareFlags(MBB, MI, Pred, LhsLo, LhsHi, RhsLo, RhsHi, MRI, DL,
                            NormPred))
      return false;

    // Materialize boolean from flags into A register.
    if (NormPred == CmpInst::ICMP_EQ) {
      // A already holds 1 (equal) or 0 (not equal).
    } else if (NormPred == CmpInst::ICMP_NE) {
      // A holds 1 if equal → flip to get NE.
      BuildMI(MBB, MI, DL, TII.get(Z80::XOR_n)).addImm(1);
    } else if (NormPred == CmpInst::ICMP_ULT) {
      // Carry flag set if LHS < RHS.
      BuildMI(MBB, MI, DL, TII.get(Z80::SBC_A_A));
      BuildMI(MBB, MI, DL, TII.get(Z80::AND_n)).addImm(1);
    } else {
      // UGE: carry clear if LHS >= RHS → invert.
      BuildMI(MBB, MI, DL, TII.get(Z80::SBC_A_A));
      BuildMI(MBB, MI, DL, TII.get(Z80::AND_n)).addImm(1);
      BuildMI(MBB, MI, DL, TII.get(Z80::XOR_n)).addImm(1);
    }

    BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg).addReg(Z80::A);

    MI.eraseFromParent();
    return true;
  }

  case Z80::G_Z80_ICMP64: {
    // 64-bit comparison via chained SUB/SBC.
    Register DstReg = MI.getOperand(0).getReg();
    auto Pred = static_cast<CmpInst::Predicate>(MI.getOperand(1).getImm());
    Register LhsW0 = MI.getOperand(2).getReg();
    Register LhsW1 = MI.getOperand(3).getReg();
    Register LhsW2 = MI.getOperand(4).getReg();
    Register LhsW3 = MI.getOperand(5).getReg();
    Register RhsW0 = MI.getOperand(6).getReg();
    Register RhsW1 = MI.getOperand(7).getReg();
    Register RhsW2 = MI.getOperand(8).getReg();
    Register RhsW3 = MI.getOperand(9).getReg();
    const DebugLoc &DL = MI.getDebugLoc();

    CmpInst::Predicate NormPred;
    if (!emit64CompareFlags(MBB, MI, Pred, LhsW0, LhsW1, LhsW2, LhsW3, RhsW0,
                            RhsW1, RhsW2, RhsW3, MRI, DL, NormPred))
      return false;

    // Materialize boolean from flags (same as G_Z80_ICMP32).
    if (NormPred == CmpInst::ICMP_EQ) {
      // A already holds 1 (equal) or 0 (not equal).
    } else if (NormPred == CmpInst::ICMP_NE) {
      BuildMI(MBB, MI, DL, TII.get(Z80::XOR_n)).addImm(1);
    } else if (NormPred == CmpInst::ICMP_ULT) {
      BuildMI(MBB, MI, DL, TII.get(Z80::SBC_A_A));
      BuildMI(MBB, MI, DL, TII.get(Z80::AND_n)).addImm(1);
    } else {
      // UGE
      BuildMI(MBB, MI, DL, TII.get(Z80::SBC_A_A));
      BuildMI(MBB, MI, DL, TII.get(Z80::AND_n)).addImm(1);
      BuildMI(MBB, MI, DL, TII.get(Z80::XOR_n)).addImm(1);
    }

    BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg).addReg(Z80::A);

    MI.eraseFromParent();
    return true;
  }

  case TargetOpcode::G_BR: {
    // Unconditional branch
    MachineBasicBlock *TargetMBB = MI.getOperand(0).getMBB();
    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::JP_nn)).addMBB(TargetMBB);
    MI.eraseFromParent();
    return true;
  }

  case TargetOpcode::G_TRAP: {
    // Lower trap to HALT instruction
    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::HALT));
    MI.eraseFromParent();
    return true;
  }

  case TargetOpcode::G_BRINDIRECT: {
    // Indirect branch: JP (HL)
    Register TargetReg = MI.getOperand(0).getReg();
    if (!RBI.constrainGenericRegister(TargetReg, Z80::GR16RegClass, MRI))
      return false;
    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::HL)
        .addReg(TargetReg);
    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::JP_HLind));
    MI.eraseFromParent();
    return true;
  }

  case TargetOpcode::G_BRCOND: {
    // Conditional branch - branch if condition is non-zero
    Register CondReg = MI.getOperand(0).getReg();
    MachineBasicBlock *TargetMBB = MI.getOperand(1).getMBB();

    // Try to fuse with G_ICMP: emit compare + conditional jump directly,
    // avoiding boolean materialization (SBC A,A; AND 1) + re-test (OR A).
    MachineInstr *CondDef = MRI.getVRegDef(CondReg);

    // Look through G_FREEZE — it's semantically a no-op that prevents
    // poison propagation but doesn't affect code generation.
    MachineInstr *FreezeMI = nullptr;
    if (CondDef && CondDef->getOpcode() == TargetOpcode::G_FREEZE &&
        MRI.hasOneNonDBGUse(CondReg)) {
      FreezeMI = CondDef;
      CondReg = CondDef->getOperand(1).getReg();
      CondDef = MRI.getVRegDef(CondReg);
    }

    if (CondDef && CondDef->getParent() == &MBB &&
        MRI.hasOneNonDBGUse(CondReg)) {
      if (CondDef->getOpcode() == TargetOpcode::G_ICMP) {
        if (emitFusedCompareAndBranch(MBB, MI, *CondDef, MRI)) {
          if (FreezeMI)
            FreezeMI->eraseFromParent();
          return true;
        }
      } else if (CondDef->getOpcode() == Z80::G_Z80_ICMP32) {
        auto Pred =
            static_cast<CmpInst::Predicate>(CondDef->getOperand(1).getImm());
        Register LhsLo = CondDef->getOperand(2).getReg();
        Register LhsHi = CondDef->getOperand(3).getReg();
        Register RhsLo = CondDef->getOperand(4).getReg();
        Register RhsHi = CondDef->getOperand(5).getReg();
        CmpInst::Predicate NormPred;
        if (emit32CompareFlags(MBB, MI, Pred, LhsLo, LhsHi, RhsLo, RhsHi, MRI,
                               MI.getDebugLoc(), NormPred,
                               /*FusedBranch=*/true)) {
          unsigned JumpOpc;
          switch (NormPred) {
          case CmpInst::ICMP_EQ:
            JumpOpc = Z80::JP_NZ_nn;
            break;
          case CmpInst::ICMP_NE:
            JumpOpc = Z80::JP_Z_nn;
            break;
          case CmpInst::ICMP_ULT:
            JumpOpc = Z80::JP_C_nn;
            break;
          default:
            JumpOpc = Z80::JP_NC_nn;
            break;
          }
          BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(JumpOpc))
              .addMBB(TargetMBB);
          CondDef->eraseFromParent();
          if (FreezeMI)
            FreezeMI->eraseFromParent();
          MI.eraseFromParent();
          return true;
        }
      } else if (CondDef->getOpcode() == Z80::G_Z80_ICMP64) {
        auto Pred =
            static_cast<CmpInst::Predicate>(CondDef->getOperand(1).getImm());
        Register LhsW0 = CondDef->getOperand(2).getReg();
        Register LhsW1 = CondDef->getOperand(3).getReg();
        Register LhsW2 = CondDef->getOperand(4).getReg();
        Register LhsW3 = CondDef->getOperand(5).getReg();
        Register RhsW0 = CondDef->getOperand(6).getReg();
        Register RhsW1 = CondDef->getOperand(7).getReg();
        Register RhsW2 = CondDef->getOperand(8).getReg();
        Register RhsW3 = CondDef->getOperand(9).getReg();
        CmpInst::Predicate NormPred;
        if (emit64CompareFlags(MBB, MI, Pred, LhsW0, LhsW1, LhsW2, LhsW3, RhsW0,
                               RhsW1, RhsW2, RhsW3, MRI, MI.getDebugLoc(),
                               NormPred,
                               /*FusedBranch=*/true)) {
          unsigned JumpOpc;
          switch (NormPred) {
          case CmpInst::ICMP_EQ:
            JumpOpc = Z80::JP_NZ_nn;
            break;
          case CmpInst::ICMP_NE:
            JumpOpc = Z80::JP_Z_nn;
            break;
          case CmpInst::ICMP_ULT:
            JumpOpc = Z80::JP_C_nn;
            break;
          default:
            JumpOpc = Z80::JP_NC_nn;
            break;
          }
          BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(JumpOpc))
              .addMBB(TargetMBB);
          CondDef->eraseFromParent();
          if (FreezeMI)
            FreezeMI->eraseFromParent();
          MI.eraseFromParent();
          return true;
        }
      }
    }

    // Fallback: test the boolean value and branch.
    if (!RBI.constrainGenericRegister(CondReg, Z80::GR8RegClass, MRI))
      return false;

    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::A)
        .addReg(CondReg);
    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::OR_A));
    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::JP_NZ_nn))
        .addMBB(TargetMBB);
    MI.eraseFromParent();
    return true;
  }

  case TargetOpcode::G_ANYEXT: {
    // Any-extend: upper bits are don't-care
    Register DstReg = MI.getOperand(0).getReg();
    Register SrcReg = MI.getOperand(1).getReg();
    const LLT DstTy = MRI.getType(DstReg);
    const LLT SrcTy = MRI.getType(SrcReg);

    if (SrcTy.getSizeInBits() <= 8 && DstTy.getSizeInBits() <= 8) {
      // s1->s8: just a COPY (s1 already lives in an 8-bit register)
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI) ||
          !RBI.constrainGenericRegister(SrcReg, Z80::GR8RegClass, MRI))
        return false;
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
          .addReg(SrcReg);
      MI.eraseFromParent();
      return true;
    }
    if (SrcTy.getSizeInBits() <= 8 && DstTy.getSizeInBits() <= 16) {
      // s8->s16 or s1->s16: copy low byte to L, H is don't-care.
      // Use implicit-def on H so register allocator knows it's defined.
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI) ||
          !RBI.constrainGenericRegister(SrcReg, Z80::GR8RegClass, MRI))
        return false;
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::L)
          .addReg(SrcReg)
          .addReg(Z80::H, RegState::ImplicitDefine);
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
          .addReg(Z80::HL);
      MI.eraseFromParent();
      return true;
    }
    return false;
  }

  case TargetOpcode::G_ZEXT: {
    Register DstReg = MI.getOperand(0).getReg();
    Register SrcReg = MI.getOperand(1).getReg();
    const LLT DstTy = MRI.getType(DstReg);
    const LLT SrcTy = MRI.getType(SrcReg);

    if (SrcTy.getSizeInBits() <= 8 && DstTy.getSizeInBits() <= 8) {
      // s1->s8: COPY (value is already 0 or 1 in an 8-bit register)
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI) ||
          !RBI.constrainGenericRegister(SrcReg, Z80::GR8RegClass, MRI))
        return false;
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
          .addReg(SrcReg);
      MI.eraseFromParent();
      return true;
    }
    if (SrcTy.getSizeInBits() <= 8 && DstTy.getSizeInBits() <= 16) {
      // Zero extend 8-bit to 16-bit via pseudo (expanded post-RA)
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI) ||
          !RBI.constrainGenericRegister(SrcReg, Z80::GR8RegClass, MRI))
        return false;
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::ZEXT_GR8_GR16), DstReg)
          .addReg(SrcReg);
      MI.eraseFromParent();
      return true;
    }
    return false;
  }

  case TargetOpcode::G_SEXT: {
    Register DstReg = MI.getOperand(0).getReg();
    Register SrcReg = MI.getOperand(1).getReg();
    const LLT DstTy = MRI.getType(DstReg);
    const LLT SrcTy = MRI.getType(SrcReg);

    if (SrcTy.getSizeInBits() <= 8 && DstTy.getSizeInBits() <= 8) {
      // s1->s8: COPY
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI) ||
          !RBI.constrainGenericRegister(SrcReg, Z80::GR8RegClass, MRI))
        return false;
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
          .addReg(SrcReg);
      MI.eraseFromParent();
      return true;
    }
    if (SrcTy.getSizeInBits() <= 8 && DstTy.getSizeInBits() <= 16) {
      // Sign extend 8-bit to 16-bit via pseudo (expanded post-RA)
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI) ||
          !RBI.constrainGenericRegister(SrcReg, Z80::GR8RegClass, MRI))
        return false;

      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::SEXT_GR8_GR16), DstReg)
          .addReg(SrcReg);
      MI.eraseFromParent();
      return true;
    }
    return false;
  }

  case TargetOpcode::G_TRUNC: {
    // Truncate - just take the low byte
    Register DstReg = MI.getOperand(0).getReg();
    Register SrcReg = MI.getOperand(1).getReg();

    if (MRI.getType(DstReg).getSizeInBits() == 8 &&
        MRI.getType(SrcReg).getSizeInBits() == 16) {
      // Try to fold trunc(sdiv/srem(sext i8, sext i8)) → 8-bit div/rem
      // directly. Since GlobalISel selects in reverse order, G_SDIV/G_SREM
      // hasn't been selected yet, so we can inspect and consume it.
      MachineInstr *SrcDef = MRI.getVRegDef(SrcReg);
      if (SrcDef &&
          (SrcDef->getOpcode() == TargetOpcode::G_SDIV ||
           SrcDef->getOpcode() == TargetOpcode::G_SREM) &&
          MRI.hasOneNonDBGUse(SrcReg)) {
        bool IsSDiv = SrcDef->getOpcode() == TargetOpcode::G_SDIV;
        Register DivSrc1 = SrcDef->getOperand(1).getReg();
        Register DivSrc2 = SrcDef->getOperand(2).getReg();
        MachineInstr *Ext1 = MRI.getVRegDef(DivSrc1);
        MachineInstr *Ext2 = MRI.getVRegDef(DivSrc2);
        if (Ext1 && Ext2 && Ext1->getOpcode() == TargetOpcode::G_SEXT &&
            Ext2->getOpcode() == TargetOpcode::G_SEXT) {
          Register Orig1 = Ext1->getOperand(1).getReg();
          Register Orig2 = Ext2->getOperand(1).getReg();
          if (MRI.getType(Orig1).getSizeInBits() == 8 &&
              MRI.getType(Orig2).getSizeInBits() == 8) {
            // Found the pattern! Emit 8-bit signed division directly.
            if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI) ||
                !RBI.constrainGenericRegister(Orig1, Z80::GR8RegClass, MRI) ||
                !RBI.constrainGenericRegister(Orig2, Z80::GR8RegClass, MRI))
              return false;

            const DebugLoc &DL = MI.getDebugLoc();

            if (MF.getFunction().hasMinSize()) {
              // -Oz: sign-extend to i16, call __divhi3/__modhi3, truncate.
              const char *FuncName = IsSDiv ? "__divhi3" : "__modhi3";
              Module *M = const_cast<Module *>(MF.getFunction().getParent());
              FunctionCallee Func = M->getOrInsertFunction(
                  FuncName,
                  FunctionType::get(Type::getInt16Ty(M->getContext()),
                                    {Type::getInt16Ty(M->getContext()),
                                     Type::getInt16Ty(M->getContext())},
                                    false));
              GlobalValue *GV = cast<GlobalValue>(Func.getCallee());

              const auto &STI = MF.getSubtarget<Z80Subtarget>();
              if (STI.hasSM83()) {
                BuildMI(MBB, MI, DL, TII.get(Z80::SEXT_GR8_GR16), Z80::DE)
                    .addReg(Orig1);
                BuildMI(MBB, MI, DL, TII.get(Z80::SEXT_GR8_GR16), Z80::BC)
                    .addReg(Orig2);
                BuildMI(MBB, MI, DL, TII.get(Z80::CALL_nn))
                    .addGlobalAddress(GV)
                    .addUse(Z80::DE, RegState::Implicit)
                    .addUse(Z80::BC, RegState::Implicit);
                BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg)
                    .addReg(Z80::C);
              } else {
                BuildMI(MBB, MI, DL, TII.get(Z80::SEXT_GR8_GR16), Z80::HL)
                    .addReg(Orig1);
                BuildMI(MBB, MI, DL, TII.get(Z80::SEXT_GR8_GR16), Z80::DE)
                    .addReg(Orig2);
                BuildMI(MBB, MI, DL, TII.get(Z80::CALL_nn))
                    .addGlobalAddress(GV)
                    .addUse(Z80::HL, RegState::Implicit)
                    .addUse(Z80::DE, RegState::Implicit);
                BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg)
                    .addReg(Z80::E);
              }
            } else {
              // Inline 8-bit signed division — result directly in i8, no SEXT.
              BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A)
                  .addReg(Orig1);
              BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::E)
                  .addReg(Orig2);
              BuildMI(MBB, MI, DL, TII.get(IsSDiv ? Z80::SDIV8 : Z80::SMOD8));
              BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg)
                  .addReg(Z80::A);
            }

            SrcDef->eraseFromParent(); // erase G_SDIV/G_SREM
            MI.eraseFromParent();      // erase G_TRUNC
            return true;
          }
        }
      }
    }

    // Dst is s1 or s8 → lives in GR8.
    // Src is s8 → GR8 (same class, just COPY).
    // Src is s16 → GR16 (extract low byte via sub_lo).
    unsigned DstBits = MRI.getType(DstReg).getSizeInBits();
    unsigned SrcBits = MRI.getType(SrcReg).getSizeInBits();

    if (DstBits > 8 || SrcBits > 16)
      return false;

    if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI))
      return false;

    if (SrcBits <= 8) {
      // s8 → s1: both in GR8, just COPY
      if (!RBI.constrainGenericRegister(SrcReg, Z80::GR8RegClass, MRI))
        return false;
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
          .addReg(SrcReg);
    } else {
      // s16 → s8/s1: extract low byte
      if (!RBI.constrainGenericRegister(SrcReg, Z80::GR16RegClass, MRI))
        return false;
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::HL)
          .addReg(SrcReg);
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
          .addReg(Z80::L);
    }
    MI.eraseFromParent();
    return true;
  }

  case TargetOpcode::G_IMPLICIT_DEF: {
    Register DstReg = MI.getOperand(0).getReg();
    const LLT DstTy = MRI.getType(DstReg);

    // Constrain the register based on size
    if (DstTy.getSizeInBits() <= 8) {
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI))
        return false;
    } else if (DstTy.getSizeInBits() <= 16) {
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI))
        return false;
    }
    // Convert to target-independent IMPLICIT_DEF (keeps the vreg defined)
    MI.setDesc(TII.get(TargetOpcode::IMPLICIT_DEF));
    return true;
  }

  case TargetOpcode::G_PHI: {
    // PHI nodes become machine PHI nodes
    const DebugLoc &DL = MI.getDebugLoc();
    Register DstReg = MI.getOperand(0).getReg();
    const LLT DstTy = MRI.getType(DstReg);

    // Constrain the destination and all incoming values
    if (DstTy.getSizeInBits() <= 8) {
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI))
        return false;
      for (unsigned i = 1; i < MI.getNumOperands(); i += 2) {
        Register SrcReg = MI.getOperand(i).getReg();
        if (!RBI.constrainGenericRegister(SrcReg, Z80::GR8RegClass, MRI))
          return false;
      }
    } else if (DstTy.getSizeInBits() <= 16) {
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI))
        return false;
      for (unsigned i = 1; i < MI.getNumOperands(); i += 2) {
        Register SrcReg = MI.getOperand(i).getReg();
        if (!RBI.constrainGenericRegister(SrcReg, Z80::GR16RegClass, MRI))
          return false;
      }
    }

    MachineInstrBuilder MIB =
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::PHI), DstReg);
    for (unsigned i = 1; i < MI.getNumOperands(); i += 2) {
      MIB.addReg(MI.getOperand(i).getReg());
      MIB.addMBB(MI.getOperand(i + 1).getMBB());
    }
    MI.eraseFromParent();
    return true;
  }

  case TargetOpcode::G_UNMERGE_VALUES: {
    Register LoReg = MI.getOperand(0).getReg();
    Register HiReg = MI.getOperand(1).getReg();
    Register SrcReg = MI.getOperand(2).getReg();
    const LLT LoTy = MRI.getType(LoReg);

    if (LoTy.getSizeInBits() > 8)
      llvm_unreachable("s32+ G_UNMERGE_VALUES must be folded by combiner "
                       "before instruction selection");

    // s8+s8 from s16: extract low and high bytes using sub-register COPYs
    if (!RBI.constrainGenericRegister(LoReg, Z80::GR8RegClass, MRI) ||
        !RBI.constrainGenericRegister(HiReg, Z80::GR8RegClass, MRI) ||
        !RBI.constrainGenericRegister(SrcReg, Z80::GR16RegClass, MRI))
      return false;

    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), LoReg)
        .addReg(SrcReg, RegState{}, Z80::sub_lo);
    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), HiReg)
        .addReg(SrcReg, RegState{}, Z80::sub_hi);
    MI.eraseFromParent();
    return true;
  }

  case TargetOpcode::G_MERGE_VALUES:
  case TargetOpcode::G_BUILD_VECTOR: {
    Register DstReg = MI.getOperand(0).getReg();
    const LLT DstTy = MRI.getType(DstReg);

    if (DstTy.getSizeInBits() > 16)
      llvm_unreachable("s32+ G_MERGE_VALUES must be folded by combiner "
                       "before instruction selection");

    // s16 from s8+s8: combine using REG_SEQUENCE.
    Register LoReg = MI.getOperand(1).getReg();
    Register HiReg = MI.getOperand(2).getReg();

    if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(LoReg, Z80::GR8RegClass, MRI) ||
        !RBI.constrainGenericRegister(HiReg, Z80::GR8RegClass, MRI))
      return false;

    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::REG_SEQUENCE),
            DstReg)
        .addReg(LoReg)
        .addImm(Z80::sub_lo)
        .addReg(HiReg)
        .addImm(Z80::sub_hi);
    MI.eraseFromParent();
    return true;
  }

  case TargetOpcode::G_UADDO: {
    // Unsigned add with overflow detection
    // %result, %overflow = G_UADDO %a, %b
    Register DstReg = MI.getOperand(0).getReg();
    Register OverflowReg = MI.getOperand(1).getReg();
    Register Src1Reg = MI.getOperand(2).getReg();
    Register Src2Reg = MI.getOperand(3).getReg();
    const LLT DstTy = MRI.getType(DstReg);

    if (DstTy.getSizeInBits() <= 16) {
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI) ||
          !RBI.constrainGenericRegister(Src1Reg, Z80::GR16_BCDERegClass, MRI) ||
          !RBI.constrainGenericRegister(Src2Reg, Z80::GR16RegClass, MRI))
        return false;

      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::HL)
          .addReg(Src2Reg);

      if (!MRI.use_nodbg_empty(OverflowReg)) {
        if (!RBI.constrainGenericRegister(OverflowReg, Z80::GR8RegClass, MRI))
          return false;
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::ADD_HL_rr_CO))
            .addReg(Src1Reg);
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Z80::HL);
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY),
                OverflowReg)
            .addReg(Z80::A);
      } else {
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::ADD_HL_rr))
            .addReg(Src1Reg);
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Z80::HL);
      }
      MI.eraseFromParent();
      return true;
    }
    return false;
  }

  case TargetOpcode::G_SADDO: {
    // Signed add with overflow detection
    // %result, %overflow = G_SADDO %a, %b
    Register DstReg = MI.getOperand(0).getReg();
    Register OverflowReg = MI.getOperand(1).getReg();
    Register Src1Reg = MI.getOperand(2).getReg();
    Register Src2Reg = MI.getOperand(3).getReg();
    const LLT DstTy = MRI.getType(DstReg);
    const auto &STI = MF.getSubtarget<Z80Subtarget>();

    if (DstTy.getSizeInBits() <= 16) {
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI) ||
          !RBI.constrainGenericRegister(Src1Reg, Z80::GR16RegClass, MRI) ||
          !RBI.constrainGenericRegister(Src2Reg, Z80::GR16_BCDERegClass, MRI))
        return false;

      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::HL)
          .addReg(Src1Reg);

      if (STI.hasSM83()) {
        // SM83: combined add + overflow detection (no P/V flag).
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::SM83_SADDO_HL_rr))
            .addReg(Src2Reg);
      } else {
        // Z80: AND A; ADC HL,rr — sets P/V for signed overflow.
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::SADD_HL_rr))
            .addReg(Src2Reg);
      }

      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
          .addReg(Z80::HL);

      if (!MRI.use_nodbg_empty(OverflowReg)) {
        if (!RBI.constrainGenericRegister(OverflowReg, Z80::GR8RegClass, MRI))
          return false;
        if (STI.hasSM83()) {
          // SM83: overflow already in A from SM83_SADDO_HL_rr.
        } else {
          // Z80: CAPTURE_PV reads P/V flag (bit 2 of F) into A as 0 or 1.
          BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::CAPTURE_PV));
        }
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY),
                OverflowReg)
            .addReg(Z80::A);
      }
      MI.eraseFromParent();
      return true;
    }
    return false;
  }

  case TargetOpcode::G_SSUBO: {
    // Signed subtract with overflow detection
    // %result, %overflow = G_SSUBO %a, %b
    Register DstReg = MI.getOperand(0).getReg();
    Register OverflowReg = MI.getOperand(1).getReg();
    Register Src1Reg = MI.getOperand(2).getReg();
    Register Src2Reg = MI.getOperand(3).getReg();
    const LLT DstTy = MRI.getType(DstReg);
    const auto &STI = MF.getSubtarget<Z80Subtarget>();

    if (DstTy.getSizeInBits() <= 16) {
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI) ||
          !RBI.constrainGenericRegister(Src1Reg, Z80::GR16RegClass, MRI) ||
          !RBI.constrainGenericRegister(Src2Reg, Z80::GR16_BCDERegClass, MRI))
        return false;

      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::HL)
          .addReg(Src1Reg);

      if (STI.hasSM83()) {
        // SM83: combined sub + overflow detection (no P/V flag).
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::SM83_SSUBO_HL_rr))
            .addReg(Src2Reg);
      } else {
        // Z80: AND A; SBC HL,rr — sets P/V for signed overflow.
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::SUB_HL_rr))
            .addReg(Src2Reg);
      }

      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
          .addReg(Z80::HL);

      if (!MRI.use_nodbg_empty(OverflowReg)) {
        if (!RBI.constrainGenericRegister(OverflowReg, Z80::GR8RegClass, MRI))
          return false;
        if (STI.hasSM83()) {
          // SM83: overflow already in A from SM83_SSUBO_HL_rr.
        } else {
          // Z80: CAPTURE_PV reads P/V flag (bit 2 of F) into A as 0 or 1.
          BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::CAPTURE_PV));
        }
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY),
                OverflowReg)
            .addReg(Z80::A);
      }
      MI.eraseFromParent();
      return true;
    }
    return false;
  }

  case TargetOpcode::G_UADDE: {
    // Unsigned add with carry in/out (for chaining)
    // %result, %carry_out = G_UADDE %a, %b, %carry_in
    Register DstReg = MI.getOperand(0).getReg();
    Register CarryOutReg = MI.getOperand(1).getReg();
    Register Src1Reg = MI.getOperand(2).getReg();
    Register Src2Reg = MI.getOperand(3).getReg();
    Register CarryInReg = MI.getOperand(4).getReg();
    const LLT DstTy = MRI.getType(DstReg);

    if (DstTy.getSizeInBits() <= 16) {
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI) ||
          !RBI.constrainGenericRegister(CarryInReg, Z80::GR8RegClass, MRI) ||
          !RBI.constrainGenericRegister(Src1Reg, Z80::GR16RegClass, MRI) ||
          !RBI.constrainGenericRegister(Src2Reg, Z80::GR16_BCDERegClass, MRI))
        return false;

      // Use atomic pseudo ADC_HL_rr_CIO which combines carry restoration
      // (LD A,carry; RRCA) + ADC HL,rr + carry capture (SBC A,A; AND 1)
      // into a single indivisible instruction.
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::HL)
          .addReg(Src1Reg);
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::ADC_HL_rr_CIO))
          .addReg(Src2Reg)
          .addReg(CarryInReg);
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
          .addReg(Z80::HL);

      // Carry out is always captured inside the atomic pseudo (in A).
      // Copy it to the virtual register only if used.
      if (!MRI.use_nodbg_empty(CarryOutReg)) {
        if (!RBI.constrainGenericRegister(CarryOutReg, Z80::GR8RegClass, MRI))
          return false;
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY),
                CarryOutReg)
            .addReg(Z80::A);
      }
      MI.eraseFromParent();
      return true;
    }
    return false;
  }

  case TargetOpcode::G_USUBO: {
    // Unsigned subtract with overflow (borrow) detection
    Register DstReg = MI.getOperand(0).getReg();
    Register OverflowReg = MI.getOperand(1).getReg();
    Register Src1Reg = MI.getOperand(2).getReg();
    Register Src2Reg = MI.getOperand(3).getReg();
    const LLT DstTy = MRI.getType(DstReg);

    if (DstTy.getSizeInBits() <= 16) {
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI) ||
          !RBI.constrainGenericRegister(Src1Reg, Z80::GR16RegClass, MRI) ||
          !RBI.constrainGenericRegister(Src2Reg, Z80::GR16_BCDERegClass, MRI))
        return false;

      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::HL)
          .addReg(Src1Reg);

      if (!MRI.use_nodbg_empty(OverflowReg)) {
        // Borrow output needed: use atomic pseudo that combines
        // AND A; SBC HL,rr + borrow capture (SBC A,A; AND 1).
        if (!RBI.constrainGenericRegister(OverflowReg, Z80::GR8RegClass, MRI))
          return false;
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::SUB_HL_rr_BO))
            .addReg(Src2Reg);
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Z80::HL);
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY),
                OverflowReg)
            .addReg(Z80::A);
      } else {
        // No borrow output needed: plain subtraction.
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::SUB_HL_rr))
            .addReg(Src2Reg);
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Z80::HL);
      }
      MI.eraseFromParent();
      return true;
    }
    return false;
  }

  case TargetOpcode::G_USUBE: {
    // Unsigned subtract with borrow in/out (for chaining)
    Register DstReg = MI.getOperand(0).getReg();
    Register BorrowOutReg = MI.getOperand(1).getReg();
    Register Src1Reg = MI.getOperand(2).getReg();
    Register Src2Reg = MI.getOperand(3).getReg();
    Register BorrowInReg = MI.getOperand(4).getReg();
    const LLT DstTy = MRI.getType(DstReg);

    if (DstTy.getSizeInBits() <= 16) {
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI) ||
          !RBI.constrainGenericRegister(BorrowInReg, Z80::GR8RegClass, MRI) ||
          !RBI.constrainGenericRegister(Src1Reg, Z80::GR16RegClass, MRI) ||
          !RBI.constrainGenericRegister(Src2Reg, Z80::GR16_BCDERegClass, MRI))
        return false;

      // Use atomic pseudo SBC_HL_rr_BIO which combines borrow restoration
      // (LD A,borrow; RRCA) + SBC HL,rr + borrow capture (SBC A,A; AND 1).
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::HL)
          .addReg(Src1Reg);
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::SBC_HL_rr_BIO))
          .addReg(Src2Reg)
          .addReg(BorrowInReg);
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
          .addReg(Z80::HL);

      if (!MRI.use_nodbg_empty(BorrowOutReg)) {
        if (!RBI.constrainGenericRegister(BorrowOutReg, Z80::GR8RegClass, MRI))
          return false;
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY),
                BorrowOutReg)
            .addReg(Z80::A);
      }
      MI.eraseFromParent();
      return true;
    }
    return false;
  }

  case TargetOpcode::G_MUL:
    if (selectMulByConst(MI))
      return true;
    if (selectMul8(MI))
      return true;
    if (STI.inlineI16Runtime())
      return selectInline16(MI, Z80::MUL16);
    return selectRuntimeLibCall16(MI, "__mulhi3");

  case TargetOpcode::G_UMULH:
    return selectRuntimeLibCall16(MI, "__umulhi3");

  case TargetOpcode::G_SDIV:
    if (selectSDivMod8(MI, /*IsDiv=*/true))
      return true;
    if (tryNarrowSDivMod16(MI, /*IsDiv=*/true))
      return true;
    if (STI.inlineI16Runtime())
      return selectInline16(MI, Z80::SDIV16);
    return selectRuntimeLibCall16(MI, "__divhi3");

  case TargetOpcode::G_UDIV:
    if (selectUDivMod8(MI, /*IsDiv=*/true))
      return true;
    if (STI.inlineI16Runtime())
      return selectInline16(MI, Z80::UDIV16);
    return selectRuntimeLibCall16(MI, "__udivhi3");

  case TargetOpcode::G_SREM:
    if (selectSDivMod8(MI, /*IsDiv=*/false))
      return true;
    if (tryNarrowSDivMod16(MI, /*IsDiv=*/false))
      return true;
    if (STI.inlineI16Runtime())
      return selectInline16(MI, Z80::SMOD16);
    return selectRuntimeLibCall16(MI, "__modhi3");

  case TargetOpcode::G_UREM:
    if (selectUDivMod8(MI, /*IsDiv=*/false))
      return true;
    if (STI.inlineI16Runtime())
      return selectInline16(MI, Z80::UMOD16);
    return selectRuntimeLibCall16(MI, "__umodhi3");

  case TargetOpcode::G_UADDSAT:
  case TargetOpcode::G_USUBSAT:
  case TargetOpcode::G_SADDSAT:
  case TargetOpcode::G_SSUBSAT: {
    // i8 saturating arithmetic — select to pseudo for ExpandPseudo.
    Register DstReg = MI.getOperand(0).getReg();
    Register Src1Reg = MI.getOperand(1).getReg();
    Register Src2Reg = MI.getOperand(2).getReg();

    if (MRI.getType(DstReg).getSizeInBits() != 8)
      return false;

    if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI) ||
        !RBI.constrainGenericRegister(Src1Reg, Z80::GR8RegClass, MRI) ||
        !RBI.constrainGenericRegister(Src2Reg, Z80::GR8RegClass, MRI))
      return false;

    unsigned PseudoOpc;
    switch (MI.getOpcode()) {
    case TargetOpcode::G_UADDSAT:
      PseudoOpc = Z80::UADDSAT8;
      break;
    case TargetOpcode::G_USUBSAT:
      PseudoOpc = Z80::USUBSAT8;
      break;
    case TargetOpcode::G_SADDSAT:
      PseudoOpc = Z80::SADDSAT8;
      break;
    case TargetOpcode::G_SSUBSAT:
      PseudoOpc = Z80::SSUBSAT8;
      break;
    default:
      llvm_unreachable("unexpected sat opcode");
    }

    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::A)
        .addReg(Src1Reg);
    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(PseudoOpc)).addReg(Src2Reg);
    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
        .addReg(Z80::A);
    MI.eraseFromParent();
    return true;
  }

  case TargetOpcode::G_INTRINSIC:
  case TargetOpcode::G_INTRINSIC_W_SIDE_EFFECTS: {
    // Handle Z80-specific intrinsics
    unsigned IntrinsicID = cast<GIntrinsic>(MI).getIntrinsicID();

    switch (IntrinsicID) {
    case Intrinsic::z80_in: {
      // i8 @llvm.z80.in(i8 port)
      // IN A,(C) where C contains the port number
      Register DstReg = MI.getOperand(0).getReg();
      Register PortReg = MI.getOperand(2).getReg();

      // Constrain port register to C
      if (!RBI.constrainGenericRegister(PortReg, Z80::GR8RegClass, MRI))
        return false;
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI))
        return false;

      // Move port to C if not already there
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::C)
          .addReg(PortReg);

      // IN A,(C)
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::IN_A_C))
          .addDef(Z80::A, RegState::Implicit);

      // Copy result from A to destination
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
          .addReg(Z80::A);

      MI.eraseFromParent();
      return true;
    }

    case Intrinsic::z80_out: {
      // void @llvm.z80.out(i8 port, i8 value)
      // OUT (C),A where C contains the port number
      Register PortReg = MI.getOperand(1).getReg();
      Register ValueReg = MI.getOperand(2).getReg();

      // Constrain registers
      if (!RBI.constrainGenericRegister(PortReg, Z80::GR8RegClass, MRI))
        return false;
      if (!RBI.constrainGenericRegister(ValueReg, Z80::GR8RegClass, MRI))
        return false;

      // Move port to C
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::C)
          .addReg(PortReg);

      // Move value to A
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::A)
          .addReg(ValueReg);

      // OUT (C),A
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::OUT_C_A));

      MI.eraseFromParent();
      return true;
    }

    case Intrinsic::z80_halt: {
      // void @llvm.z80.halt()
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::HALT));
      MI.eraseFromParent();
      return true;
    }

    case Intrinsic::z80_di: {
      // void @llvm.z80.di()
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::DI));
      MI.eraseFromParent();
      return true;
    }

    case Intrinsic::z80_ei: {
      // void @llvm.z80.ei()
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::EI));
      MI.eraseFromParent();
      return true;
    }

    case Intrinsic::z80_nop: {
      // void @llvm.z80.nop()
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::NOP));
      MI.eraseFromParent();
      return true;
    }

    default:
      return false;
    }
  }
  }

  return false;
}

InstructionSelector *llvm::createZ80InstructionSelector(
    const Z80TargetMachine &TM, Z80Subtarget &STI, Z80RegisterBankInfo &RBI) {
  return new Z80InstructionSelector(TM, STI, RBI);
}
