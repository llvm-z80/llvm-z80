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
#include "Z80InstrInfo.h"
#include "Z80RegisterInfo.h"
#include "Z80Subtarget.h"

#include "llvm/CodeGen/GlobalISel/GIMatchTableExecutor.h"
#include "llvm/CodeGen/GlobalISel/GenericMachineInstrs.h"
#include "llvm/CodeGen/GlobalISel/InstructionSelector.h"
#include "llvm/CodeGen/GlobalISel/MachineIRBuilder.h"
#include "llvm/CodeGen/GlobalISel/Utils.h"
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

#include <optional>

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
  bool emitFusedCompareAndBranch(MachineBasicBlock &MBB, MachineInstr &MI,
                                 MachineInstr &CmpMI, MachineRegisterInfo &MRI);
  std::optional<Register> emitEqualityTest(MachineBasicBlock &MBB,
                                           MachineBasicBlock::iterator I,
                                           const DebugLoc &DL, Register LHS,
                                           Register RHS, bool FlagsOnly,
                                           MachineRegisterInfo &MRI);
  std::optional<Register>
  emitWideEqualityTest(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                       const DebugLoc &DL, ArrayRef<Register> LHS,
                       ArrayRef<Register> RHS, MachineRegisterInfo &MRI);
  Register emitBoolFromZero(MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator I, const DebugLoc &DL,
                            Register Diff, bool Equal,
                            MachineRegisterInfo &MRI);
  Register emitBoolFromCarry(MachineBasicBlock &MBB,
                             MachineBasicBlock::iterator I, const DebugLoc &DL,
                             bool CarrySet, MachineRegisterInfo &MRI);
  bool emitCarryOut(MachineBasicBlock &MBB, MachineInstr &MI, Register Out,
                    MachineRegisterInfo &MRI);
  Register emitAccUnary(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                        const DebugLoc &DL, unsigned Opc, Register Src,
                        MachineRegisterInfo &MRI);
  Register emitAccImm(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                      const DebugLoc &DL, unsigned Opc, Register Src,
                      uint8_t Imm, MachineRegisterInfo &MRI);
  Register emitZeroByte(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                        const DebugLoc &DL, MachineRegisterInfo &MRI);
  Register emitSignFill(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                        const DebugLoc &DL, Register Src,
                        MachineRegisterInfo &MRI);
  Register emitShlByte(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                       const DebugLoc &DL, Register Src, unsigned Amt,
                       MachineRegisterInfo &MRI);
  Register emitLshrByte(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                        const DebugLoc &DL, Register Src, unsigned Amt,
                        MachineRegisterInfo &MRI);
  Register emitAshrByte(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                        const DebugLoc &DL, Register Src, unsigned Amt,
                        MachineRegisterInfo &MRI);
  MachineInstrBuilder buildAccOp(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator I,
                                 const DebugLoc &DL, unsigned Opc, Register Dst,
                                 Register LHS, MachineRegisterInfo &MRI);
  bool emitOrderChain(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                      const DebugLoc &DL, ArrayRef<Register> LHS,
                      ArrayRef<Register> RHS, MachineRegisterInfo &MRI);
  bool emit32CompareFlags(MachineBasicBlock &MBB,
                          MachineBasicBlock::iterator InsertPt,
                          CmpInst::Predicate Pred, Register LhsLo,
                          Register LhsHi, Register RhsLo, Register RhsHi,
                          MachineRegisterInfo &MRI, const DebugLoc &DL,
                          CmpInst::Predicate &NormalizedPred,
                          Register *Bool = nullptr);
  bool emit64CompareFlags(
      MachineBasicBlock &MBB, MachineBasicBlock::iterator InsertPt,
      CmpInst::Predicate Pred, Register LhsW0, Register LhsW1, Register LhsW2,
      Register LhsW3, Register RhsW0, Register RhsW1, Register RhsW2,
      Register RhsW3, MachineRegisterInfo &MRI, const DebugLoc &DL,
      CmpInst::Predicate &NormalizedPred, Register *Bool = nullptr);

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

/// Whether selection builds \p Reg as exactly 0 or 1 in its byte. That holds
/// for what a comparison or the carry or overflow out of an arithmetic
/// operation selects to, even for a poison input, where the generic value
/// tracking has to assume a freeze may pick any byte; and it carries through
/// the operations that keep a byte a boolean.
static bool isSelectedBool(Register Reg, const MachineRegisterInfo &MRI,
                           unsigned Depth = 0) {
  if (Depth > 6)
    return false;
  const MachineInstr *Def = MRI.getVRegDef(Reg);
  if (!Def)
    return false;
  auto IsBool = [&](unsigned Idx) {
    return isSelectedBool(Def->getOperand(Idx).getReg(), MRI, Depth + 1);
  };
  switch (Def->getOpcode()) {
  case TargetOpcode::G_ICMP:
  case Z80::G_Z80_ICMP32:
  case Z80::G_Z80_ICMP64:
    return true;
  case TargetOpcode::G_UADDO:
  case TargetOpcode::G_USUBO:
  case TargetOpcode::G_UADDE:
  case TargetOpcode::G_USUBE:
  case TargetOpcode::G_SADDO:
  case TargetOpcode::G_SSUBO:
    return Def->getOperand(1).getReg() == Reg;
  case TargetOpcode::G_CONSTANT:
    return Def->getOperand(1).getCImm()->getZExtValue() <= 1;
  case TargetOpcode::G_FREEZE:
  case TargetOpcode::G_ANYEXT:
  case TargetOpcode::G_ZEXT:
  case TargetOpcode::G_TRUNC:
    return IsBool(1);
  case TargetOpcode::G_AND:
    return IsBool(1) || IsBool(2);
  case TargetOpcode::G_OR:
  case TargetOpcode::G_XOR:
    return IsBool(1) && IsBool(2);
  case TargetOpcode::G_PHI:
    for (unsigned I = 1, E = Def->getNumOperands(); I < E; I += 2)
      if (!IsBool(I))
        return false;
    return true;
  default:
    return false;
  }
}

/// The compile-time address behind \p AddrReg, or nullopt when the address is
/// not a constant. Looks through the pointer casts that select to a plain copy.
static std::optional<uint16_t> getConstantAddr(Register AddrReg,
                                               MachineRegisterInfo &MRI) {
  MachineInstr *Def = MRI.getVRegDef(AddrReg);
  while (Def &&
         (Def->getOpcode() == TargetOpcode::G_INTTOPTR ||
          Def->getOpcode() == TargetOpcode::G_PTRTOINT ||
          Def->getOpcode() == TargetOpcode::COPY) &&
         Def->getOperand(1).isReg() && Def->getOperand(1).getReg().isVirtual())
    Def = MRI.getVRegDef(Def->getOperand(1).getReg());
  if (!Def || Def->getOpcode() != TargetOpcode::G_CONSTANT)
    return std::nullopt;
  return static_cast<uint16_t>(Def->getOperand(1).getCImm()->getZExtValue() &
                               0xFFFF);
}

/// Recognize an address the linker settles: a global, or a global displaced by
/// a constant. The direct forms take such an address as an immediate, so
/// nothing has to reach a pointer register first.
static bool getGlobalAddr(Register AddrReg, MachineRegisterInfo &MRI,
                          const GlobalValue *&GV, int64_t &Offset) {
  Offset = 0;
  MachineInstr *Def = MRI.getVRegDef(AddrReg);
  while (Def) {
    switch (Def->getOpcode()) {
    case TargetOpcode::G_INTTOPTR:
    case TargetOpcode::G_PTRTOINT:
    case TargetOpcode::COPY:
      if (!Def->getOperand(1).isReg() ||
          !Def->getOperand(1).getReg().isVirtual())
        return false;
      Def = MRI.getVRegDef(Def->getOperand(1).getReg());
      continue;
    case TargetOpcode::G_PTR_ADD: {
      std::optional<int64_t> Disp =
          getIConstantVRegSExtVal(Def->getOperand(2).getReg(), MRI);
      if (!Disp)
        return false;
      Offset += *Disp;
      Def = MRI.getVRegDef(Def->getOperand(1).getReg());
      continue;
    }
    case TargetOpcode::G_GLOBAL_VALUE:
      GV = Def->getOperand(1).getGlobal();
      Offset += Def->getOperand(1).getOffset();
      // Pointer arithmetic wraps at the width of a pointer; a chain that sums
      // past it would leave an out-of-range addend in the relocation.
      Offset = static_cast<int16_t>(Offset);
      return true;
    default:
      return false;
    }
  }
  return false;
}

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
  BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::E).addReg(Src2Reg);
  buildAccOp(MBB, MI, DL, Z80::MUL8, DstReg, Src1Reg, MRI);

  MI.eraseFromParent();
  return true;
}

// Select G_UDIV/G_UREM i8: inline 8-bit restoring division via pseudo.
// Input: A = dividend, E = divisor. Output: A = quotient (UDIV8) or remainder
// (UMOD8). The call is shorter than the inline loop but costs the time of
// getting there, so only minsize takes it. SM83 calls either way: the
// instructions the loop is missing there make it lose on both counts.
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

  if (MF.getSubtarget<Z80Subtarget>().hasSM83() ||
      MF.getFunction().hasMinSize()) {
    // Call the dedicated 8-bit runtime function instead.  There is no RTLIB
    // slot for it, so the call is built by hand, but it follows
    // CallingConv::Z80_Builtin: dividend in A, divisor in the low half of the
    // first argument pair, result in A.
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
    Register DivisorReg =
        MF.getSubtarget<Z80Subtarget>().hasSM83() ? Z80::E : Z80::L;
    BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DivisorReg)
        .addReg(Src2Reg);
    BuildMI(MBB, MI, DL, TII.get(Z80::CALL_nn))
        .addGlobalAddress(GV)
        .addUse(Z80::A, RegState::Implicit)
        .addUse(DivisorReg, RegState::Implicit);
    BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg).addReg(Z80::A);

    MI.eraseFromParent();
    return true;
  }

  if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI) ||
      !RBI.constrainGenericRegister(Src1Reg, Z80::GR8RegClass, MRI) ||
      !RBI.constrainGenericRegister(Src2Reg, Z80::GR8RegClass, MRI))
    return false;

  BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::E).addReg(Src2Reg);
  buildAccOp(MBB, MI, DL, IsDiv ? Z80::UDIV8 : Z80::UMOD8, DstReg, Src1Reg,
             MRI);

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

  BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::E).addReg(Src2Reg);
  buildAccOp(MBB, MI, DL, IsDiv ? Z80::SDIV8 : Z80::SMOD8, DstReg, Src1Reg,
             MRI);

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

  BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::E).addReg(Orig2);
  Register Quot =
      emitAccUnary(MBB, MI, DL, IsDiv ? Z80::SDIV8 : Z80::SMOD8, Orig1, MRI);
  // Sign-extend the 8-bit result to the 16-bit destination.
  BuildMI(MBB, MI, DL, TII.get(Z80::SEXT_GR8_GR16), DstReg).addReg(Quot);

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
    Z80::buildLD16n(MBB, MI, DL, TII, Z80::HL).addImm(0);
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
      Z80::buildAddHL(MBB, MI, DL, TII, Z80::DE);
    }
  }

  // Copy result from HL to dst
  BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg).addReg(Z80::HL);

  MI.eraseFromParent();
  return true;
}

/// Sets the Z flag exactly when the byte or pair \p LHS equals \p RHS, for
/// a branch and a 0/1 value alike, and returns the register holding their
/// difference, zero exactly on equality. A byte compared for a branch
/// alone is compared with CP, or OR A against zero, which leave nothing to
/// return.
///
/// A pair is equal when the differences of its two bytes OR to zero. A byte
/// compared with zero is its own difference and goes into the OR as it is,
/// so a constant with a zero byte needs no register for the other.
std::optional<Register> Z80InstructionSelector::emitEqualityTest(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator I, const DebugLoc &DL,
    Register LHS, Register RHS, bool FlagsOnly, MachineRegisterInfo &MRI) {
  std::optional<int64_t> C = getIConstantVRegSExtVal(RHS, MRI);
  auto newByte = [&] { return MRI.createVirtualRegister(&Z80::GR8RegClass); };

  if (MRI.getType(LHS).getSizeInBits() <= 8) {
    if (!RBI.constrainGenericRegister(LHS, Z80::GR8RegClass, MRI) ||
        (!C && !RBI.constrainGenericRegister(RHS, Z80::GR8RegClass, MRI)))
      return std::nullopt;
    if (C && (*C & 0xFF) == 0) {
      // The byte is its own difference from zero; only a branch needs the
      // flag.
      if (!FlagsOnly)
        return LHS;
      buildAccOp(MBB, I, DL, Z80::TST_Ac, Register(), LHS, MRI);
      return Register();
    }
    Register Diff = FlagsOnly ? Register() : newByte();
    if (C)
      buildAccOp(MBB, I, DL, FlagsOnly ? Z80::CP_Ac_n : Z80::SUB_Ac_n, Diff,
                 LHS, MRI)
          .addImm(*C & 0xFF);
    else
      buildAccOp(MBB, I, DL, FlagsOnly ? Z80::CP_Ac_r : Z80::SUB_Ac_r, Diff,
                 LHS, MRI)
          .addReg(RHS);
    return Diff;
  }

  if (!RBI.constrainGenericRegister(LHS, Z80::GR16RegClass, MRI) ||
      (!C && !RBI.constrainGenericRegister(RHS, Z80::GR16RegClass, MRI)))
    return std::nullopt;
  auto byteOf = [&](Register Pair, unsigned Sub) {
    Register Byte = newByte();
    BuildMI(MBB, I, DL, TII.get(TargetOpcode::COPY), Byte)
        .addReg(Pair, RegState{}, Sub);
    return Byte;
  };
  // The difference of one byte of the pair. Only the OR it goes into sets
  // the flags that are read, so a byte of all ones is flipped with CPL.
  auto difference = [&](unsigned Sub) {
    Register Other = C ? Register() : byteOf(RHS, Sub);
    Register Byte = byteOf(LHS, Sub);
    uint8_t V = C ? (*C >> (Sub == Z80::sub_hi ? 8 : 0)) & 0xFF : 0;
    if (!Other && !V)
      return Byte;
    Register Diff = newByte();
    if (Other)
      buildAccOp(MBB, I, DL, Z80::XOR_Ac_r, Diff, Byte, MRI).addReg(Other);
    else if (V == 0xFF)
      buildAccOp(MBB, I, DL, Z80::CPL_Ac, Diff, Byte, MRI);
    else
      buildAccOp(MBB, I, DL, Z80::XOR_Ac_n, Diff, Byte, MRI).addImm(V);
    return Diff;
  };
  auto either = [&](Register L, Register R) {
    Register Or = newByte();
    buildAccOp(MBB, I, DL, Z80::OR_Ac_r, Or, L, MRI).addReg(R);
    return Or;
  };
  uint8_t Lo = C ? *C & 0xFF : 1;
  uint8_t Hi = C ? (*C >> 8) & 0xFF : 1;
  if (!Hi || !Lo) {
    Register Zero = byteOf(LHS, !Hi ? Z80::sub_hi : Z80::sub_lo);
    return either(difference(!Hi ? Z80::sub_lo : Z80::sub_hi), Zero);
  }
  Register HiDiff = difference(Z80::sub_hi);
  return either(difference(Z80::sub_lo), HiDiff);
}

/// The same for a value split into pairs: the differences of all of them,
/// ORed together, are zero and set the Z flag exactly on equality.
std::optional<Register> Z80InstructionSelector::emitWideEqualityTest(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator I, const DebugLoc &DL,
    ArrayRef<Register> LHS, ArrayRef<Register> RHS, MachineRegisterInfo &MRI) {
  Register Acc;
  for (unsigned K = 0; K != LHS.size(); ++K) {
    std::optional<Register> Diff =
        emitEqualityTest(MBB, I, DL, LHS[K], RHS[K], /*FlagsOnly=*/false, MRI);
    if (!Diff)
      return std::nullopt;
    if (!Acc) {
      Acc = *Diff;
      continue;
    }
    Register Or = MRI.createVirtualRegister(&Z80::GR8RegClass);
    buildAccOp(MBB, I, DL, Z80::OR_Ac_r, Or, *Diff, MRI).addReg(Acc);
    Acc = Or;
  }
  return Acc;
}

/// Builds \p Opc on the accumulator and returns it for any other operand to
/// be added. \p LHS goes in and the result comes out to \p Dst through
/// copies of registers in Ac: the coalescer folds them away along a chain of
/// accumulator operations, and the allocator wherever else A is free. An
/// operation that reads no accumulator takes no \p LHS, and one whose result
/// is wanted only for its flags no \p Dst.
MachineInstrBuilder Z80InstructionSelector::buildAccOp(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator I, const DebugLoc &DL,
    unsigned Opc, Register Dst, Register LHS, MachineRegisterInfo &MRI) {
  const MCInstrDesc &Desc = TII.get(Opc);
  Register In;
  if (LHS) {
    In = MRI.createVirtualRegister(&Z80::AcRegClass);
    BuildMI(MBB, I, DL, TII.get(TargetOpcode::COPY), In).addReg(LHS);
  }
  auto MIB = BuildMI(MBB, I, DL, Desc);
  Register Out;
  if (Desc.getNumDefs()) {
    Out = MRI.createVirtualRegister(&Z80::AcRegClass);
    MIB.addDef(Out);
  }
  if (In)
    MIB.addReg(In);
  if (Dst)
    BuildMI(MBB, I, DL, TII.get(TargetOpcode::COPY), Dst).addReg(Out);
  return MIB;
}

/// The 0/1 value of EQ or NE from \p Diff, zero exactly on equality: SUB 1
/// borrows only from zero and ADD 0xFF carries from anything else.
Register Z80InstructionSelector::emitBoolFromZero(MachineBasicBlock &MBB,
                                                  MachineBasicBlock::iterator I,
                                                  const DebugLoc &DL,
                                                  Register Diff, bool Equal,
                                                  MachineRegisterInfo &MRI) {
  buildAccOp(MBB, I, DL, Equal ? Z80::SUB_Ac_n : Z80::ADD_Ac_n, Register(),
             Diff, MRI)
      .addImm(Equal ? 1 : 0xFF);
  return emitBoolFromCarry(MBB, I, DL, /*CarrySet=*/true, MRI);
}

/// The 0/1 value of whether the carry is set, or clear: SBC A,A spreads the
/// carry to 0 or 0xFF, and AND 1 or INC turns that into the answer.
Register Z80InstructionSelector::emitBoolFromCarry(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator I, const DebugLoc &DL,
    bool CarrySet, MachineRegisterInfo &MRI) {
  Register Mask = MRI.createVirtualRegister(&Z80::GR8RegClass);
  Register Bool = MRI.createVirtualRegister(&Z80::GR8RegClass);
  buildAccOp(MBB, I, DL, Z80::SBC_Ac_Ac, Mask, Register(), MRI);
  if (CarrySet)
    buildAccOp(MBB, I, DL, Z80::AND_Ac_n, Bool, Mask, MRI).addImm(1);
  else
    BuildMI(MBB, I, DL, TII.get(Z80::INC_r), Bool).addReg(Mask);
  return Bool;
}

/// Gives \p Out, the carry or borrow out of the 16-bit arithmetic just built
/// before \p MI, its 0/1 value, if anything reads it. It comes right after
/// the arithmetic, before the result leaves HL, so the allocator has nowhere
/// to put a spill while the carry waits in the flags.
bool Z80InstructionSelector::emitCarryOut(MachineBasicBlock &MBB,
                                          MachineInstr &MI, Register Out,
                                          MachineRegisterInfo &MRI) {
  if (MRI.use_nodbg_empty(Out))
    return true;
  if (!RBI.constrainGenericRegister(Out, Z80::GR8RegClass, MRI))
    return false;
  Register Carry = emitBoolFromCarry(MBB, MI, MI.getDebugLoc(),
                                     /*CarrySet=*/true, MRI);
  BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Out)
      .addReg(Carry);
  return true;
}

/// \p Opc on the accumulator applied to \p Src, into a new register.
Register Z80InstructionSelector::emitAccUnary(MachineBasicBlock &MBB,
                                              MachineBasicBlock::iterator I,
                                              const DebugLoc &DL, unsigned Opc,
                                              Register Src,
                                              MachineRegisterInfo &MRI) {
  Register Dst = MRI.createVirtualRegister(&Z80::GR8RegClass);
  buildAccOp(MBB, I, DL, Opc, Dst, Src, MRI);
  return Dst;
}

/// \p Opc on the accumulator applied to \p Src and \p Imm, into a new
/// register.
Register Z80InstructionSelector::emitAccImm(MachineBasicBlock &MBB,
                                            MachineBasicBlock::iterator I,
                                            const DebugLoc &DL, unsigned Opc,
                                            Register Src, uint8_t Imm,
                                            MachineRegisterInfo &MRI) {
  Register Dst = MRI.createVirtualRegister(&Z80::GR8RegClass);
  buildAccOp(MBB, I, DL, Opc, Dst, Src, MRI).addImm(Imm);
  return Dst;
}

/// A zero byte, in whichever register the allocator likes.
Register Z80InstructionSelector::emitZeroByte(MachineBasicBlock &MBB,
                                              MachineBasicBlock::iterator I,
                                              const DebugLoc &DL,
                                              MachineRegisterInfo &MRI) {
  Register Dst = MRI.createVirtualRegister(&Z80::GR8RegClass);
  BuildMI(MBB, I, DL, TII.get(Z80::LD_r_n), Dst).addImm(0);
  return Dst;
}

/// The sign of \p Src spread over a byte: ADD A,A moves it into the carry
/// and SBC A,A spreads the carry.
Register Z80InstructionSelector::emitSignFill(MachineBasicBlock &MBB,
                                              MachineBasicBlock::iterator I,
                                              const DebugLoc &DL, Register Src,
                                              MachineRegisterInfo &MRI) {
  buildAccOp(MBB, I, DL, Z80::ADD_Ac_Ac, Register(), Src, MRI);
  Register Dst = MRI.createVirtualRegister(&Z80::GR8RegClass);
  buildAccOp(MBB, I, DL, Z80::SBC_Ac_Ac, Dst, Register(), MRI);
  return Dst;
}

// The byte shifts below pick the shortest sequence. ADD A,A doubles in one
// byte; SRL and SRA take two but work in any register. Past the middle of the
// byte a rotate the other way and a mask is shorter still, and SM83's SWAP
// moves a nibble at once.

/// \p Src shifted left by \p Amt within a byte.
Register Z80InstructionSelector::emitShlByte(MachineBasicBlock &MBB,
                                             MachineBasicBlock::iterator I,
                                             const DebugLoc &DL, Register Src,
                                             unsigned Amt,
                                             MachineRegisterInfo &MRI) {
  if (Amt >= 8)
    return emitZeroByte(MBB, I, DL, MRI);
  Register V = Src;
  if (Amt >= 6) {
    for (unsigned K = Amt; K != 8; ++K)
      V = emitAccUnary(MBB, I, DL, Z80::RRCA_Ac, V, MRI);
    return emitAccImm(MBB, I, DL, Z80::AND_Ac_n, V, (0xFF << Amt) & 0xFF, MRI);
  }
  for (unsigned K = 0; K != Amt; ++K)
    V = emitAccUnary(MBB, I, DL, Z80::ADD_Ac_Ac, V, MRI);
  return V;
}

/// \p Src shifted right by \p Amt within a byte, filling with zeros.
Register Z80InstructionSelector::emitLshrByte(MachineBasicBlock &MBB,
                                              MachineBasicBlock::iterator I,
                                              const DebugLoc &DL, Register Src,
                                              unsigned Amt,
                                              MachineRegisterInfo &MRI) {
  if (Amt >= 8)
    return emitZeroByte(MBB, I, DL, MRI);
  bool IsSM83 = MBB.getParent()->getSubtarget<Z80Subtarget>().hasSM83();
  if (Amt == 4 && IsSM83)
    return emitAccImm(MBB, I, DL, Z80::AND_Ac_n,
                      emitAccUnary(MBB, I, DL, Z80::SWAP_Ac, Src, MRI), 0x0F,
                      MRI);
  Register V = Src;
  if (Amt >= 4) {
    for (unsigned K = Amt; K != 8; ++K)
      V = emitAccUnary(MBB, I, DL, Z80::RLCA_Ac, V, MRI);
    return emitAccImm(MBB, I, DL, Z80::AND_Ac_n, V, 0xFF >> Amt, MRI);
  }
  for (unsigned K = 0; K != Amt; ++K) {
    Register Next = MRI.createVirtualRegister(&Z80::GR8RegClass);
    BuildMI(MBB, I, DL, TII.get(Z80::SRL_r), Next).addReg(V);
    V = Next;
  }
  return V;
}

/// \p Src shifted right by \p Amt within a byte, filling with its sign.
Register Z80InstructionSelector::emitAshrByte(MachineBasicBlock &MBB,
                                              MachineBasicBlock::iterator I,
                                              const DebugLoc &DL, Register Src,
                                              unsigned Amt,
                                              MachineRegisterInfo &MRI) {
  if (Amt >= 7)
    return emitSignFill(MBB, I, DL, Src, MRI);
  Register V = Src;
  for (unsigned K = 0; K != Amt; ++K) {
    Register Next = MRI.createVirtualRegister(&Z80::GR8RegClass);
    BuildMI(MBB, I, DL, TII.get(Z80::SRA_r), Next).addReg(V);
    V = Next;
  }
  return V;
}

/// Sets the carry exactly when \p LHS is below \p RHS, unsigned, for values
/// held as pairs from the low one up: the first pair is subtracted and each
/// one after with the borrow. A constant pair is taken as immediates, with no
/// pair of its own.
bool Z80InstructionSelector::emitOrderChain(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator I, const DebugLoc &DL,
    ArrayRef<Register> LHS, ArrayRef<Register> RHS, MachineRegisterInfo &MRI) {
  for (unsigned K = 0; K != LHS.size(); ++K) {
    if (!RBI.constrainGenericRegister(LHS[K], Z80::GR16RegClass, MRI))
      return false;
    if (std::optional<APInt> C = getIConstantVRegVal(RHS[K], MRI)) {
      BuildMI(MBB, I, DL,
              TII.get(K ? Z80::CMP16_SBC_FLAGS_IMM : Z80::CMP16_FLAGS_IMM))
          .addReg(LHS[K])
          .addImm(C->getZExtValue() & 0xFFFF);
      continue;
    }
    if (!RBI.constrainGenericRegister(RHS[K], Z80::GR16RegClass, MRI))
      return false;
    BuildMI(MBB, I, DL, TII.get(K ? Z80::CMP16_SBC_FLAGS : Z80::CMP16_FLAGS))
        .addReg(LHS[K])
        .addReg(RHS[K]);
  }
  return true;
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
    if (Pred == CmpInst::ICMP_EQ || Pred == CmpInst::ICMP_NE) {
      if (!emitEqualityTest(MBB, MI, DL, LHS, RHS, /*FlagsOnly=*/true, MRI))
        return false;
    } else if (!IsSigned) {
      // CP sets the carry when A is below its operand.
      std::optional<int64_t> C = getIConstantVRegSExtVal(RHS, MRI);
      if (!RBI.constrainGenericRegister(LHS, Z80::GR8RegClass, MRI) ||
          (!C && !RBI.constrainGenericRegister(RHS, Z80::GR8RegClass, MRI)))
        return false;
      if (C)
        buildAccOp(MBB, MI, DL, Z80::CP_Ac_n, Register(), LHS, MRI)
            .addImm(*C & 0xFF);
      else
        buildAccOp(MBB, MI, DL, Z80::CP_Ac_r, Register(), LHS, MRI).addReg(RHS);
    } else {
      // The legalizer leaves a signed order only as x < 0 or x >= 0, which
      // asks for the sign bit alone. A value that dies here is shifted out of
      // A, a byte shorter when it is there already; one still needed is read
      // in place by BIT, which leaves A alone.
      std::optional<int64_t> RC = getIConstantVRegSExtVal(RHS, MRI);
      if (!RC || *RC != 0 ||
          !RBI.constrainGenericRegister(LHS, Z80::GR8RegClass, MRI))
        return false;
      bool Negative = Pred == CmpInst::ICMP_SLT;
      if (MRI.hasOneNonDBGUse(LHS)) {
        buildAccOp(MBB, MI, DL, Z80::ADD_Ac_Ac, Register(), LHS, MRI);
        JumpOpc = Negative ? Z80::JP_C_nn : Z80::JP_NC_nn;
      } else {
        BuildMI(MBB, MI, DL, TII.get(Z80::BIT_b_r)).addImm(7).addReg(LHS);
        JumpOpc = Negative ? Z80::JP_NZ_nn : Z80::JP_Z_nn;
      }
    }
  } else if (LHSTy.getSizeInBits() <= 16) {
    if (Pred == CmpInst::ICMP_EQ || Pred == CmpInst::ICMP_NE) {
      if (!emitEqualityTest(MBB, MI, DL, LHS, RHS, /*FlagsOnly=*/true, MRI))
        return false;
    } else if (IsSigned) {
      // The legalizer leaves no signed order on pairs.
      return false;
    } else {
      // Unsigned ULT/UGE: the carry decides.
      if (!emitOrderChain(MBB, MI, DL, LHS, RHS, MRI))
        return false;
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
    CmpInst::Predicate &NormalizedPred, Register *Bool) {

  if (Pred == CmpInst::ICMP_EQ || Pred == CmpInst::ICMP_NE) {
    std::optional<Register> Diff = emitWideEqualityTest(
        MBB, InsertPt, DL, {LhsLo, LhsHi}, {RhsLo, RhsHi}, MRI);
    if (!Diff)
      return false;
    if (!Bool) {
      // Z is set on equality. Flip NormalizedPred so the caller's jump
      // mapping works correctly:
      //   EQ → NE (caller emits JP_Z → jumps when Z=1 → equal)
      //   NE → EQ (caller emits JP_NZ → jumps when Z=0 → not equal)
      NormalizedPred =
          (Pred == CmpInst::ICMP_EQ) ? CmpInst::ICMP_NE : CmpInst::ICMP_EQ;
    } else {
      *Bool = emitBoolFromZero(MBB, InsertPt, DL, *Diff,
                               Pred == CmpInst::ICMP_EQ, MRI);
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
  default:
    // The legalizer leaves no signed order on wide values.
    if (ICmpInst::isSigned(Pred))
      return false;
    break;
  }
  if (Swap) {
    std::swap(LhsLo, RhsLo);
    std::swap(LhsHi, RhsHi);
  }

  if (!emitOrderChain(MBB, InsertPt, DL, {LhsLo, LhsHi}, {RhsLo, RhsHi}, MRI))
    return false;

  NormalizedPred = Pred;
  return true;
}

bool Z80InstructionSelector::emit64CompareFlags(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator InsertPt,
    CmpInst::Predicate Pred, Register LhsW0, Register LhsW1, Register LhsW2,
    Register LhsW3, Register RhsW0, Register RhsW1, Register RhsW2,
    Register RhsW3, MachineRegisterInfo &MRI, const DebugLoc &DL,
    CmpInst::Predicate &NormalizedPred, Register *Bool) {

  if (Pred == CmpInst::ICMP_EQ || Pred == CmpInst::ICMP_NE) {
    std::optional<Register> Diff =
        emitWideEqualityTest(MBB, InsertPt, DL, {LhsW0, LhsW1, LhsW2, LhsW3},
                             {RhsW0, RhsW1, RhsW2, RhsW3}, MRI);
    if (!Diff)
      return false;
    if (!Bool) {
      // Z is set on equality; flipped as for 32 bits.
      NormalizedPred =
          (Pred == CmpInst::ICMP_EQ) ? CmpInst::ICMP_NE : CmpInst::ICMP_EQ;
    } else {
      *Bool = emitBoolFromZero(MBB, InsertPt, DL, *Diff,
                               Pred == CmpInst::ICMP_EQ, MRI);
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
  default:
    // The legalizer leaves no signed order on wide values.
    if (ICmpInst::isSigned(Pred))
      return false;
    break;
  }
  if (Swap) {
    std::swap(LhsW0, RhsW0);
    std::swap(LhsW1, RhsW1);
    std::swap(LhsW2, RhsW2);
    std::swap(LhsW3, RhsW3);
  }

  if (!emitOrderChain(MBB, InsertPt, DL, {LhsW0, LhsW1, LhsW2, LhsW3},
                      {RhsW0, RhsW1, RhsW2, RhsW3}, MRI))
    return false;

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
    MIB.addFrameIndex(FIInfo.FI).addImm(FIInfo.Offset);
    // The pseudo performs the load the fold is about to erase.
    MIB.cloneMemRefs(*FIInfo.LoadMI);
    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
        .addReg(Z80::HL);
    moveLifetimeEnd(FIInfo.LoadMI, MI,
                    std::next(MachineBasicBlock::iterator(*MIB.getInstr())),
                    FIInfo.FI);
    FIInfo.LoadMI->eraseFromParent();
    MI.eraseFromParent();
    return true;
  };

  // Helper: fold a single-use frame slot load into an 8-bit ALU operation.
  // ASrcReg is the operand that goes to A; FoldReg is the one whose load
  // becomes the memory operand. Only where the function keeps a frame
  // pointer, since IX+d is the form this is worth doing for; without one
  // the expansion has to unfold it again and nothing is gained.
  auto tryAluFIFold = [&](Register ASrcReg, Register FoldReg, Register DstReg,
                          unsigned AluOp) -> bool {
    const MachineFunction &FoldMF = *MBB.getParent();
    if (!FoldMF.getSubtarget().getFrameLowering()->hasFP(FoldMF))
      return false;
    FILoadInfo FIInfo = getFILoad(FoldReg);
    if (FIInfo.FI < 0)
      return false;
    if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI) ||
        !RBI.constrainGenericRegister(ASrcReg, Z80::GR8RegClass, MRI))
      return false;
    auto MIB = buildAccOp(MBB, MI, MI.getDebugLoc(), Z80::ALU_Ac_FI, DstReg,
                          ASrcReg, MRI)
                   .addImm(AluOp)
                   .addFrameIndex(FIInfo.FI)
                   .addImm(FIInfo.Offset);
    // The pseudo performs the load the fold is about to erase.
    MIB.cloneMemRefs(*FIInfo.LoadMI);
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
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::LD_r_n), DstReg)
          .addImm(Val & 0xFF);
    } else if (DstTy.getSizeInBits() <= 16) {
      // Constrain destination to 16-bit register class
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI))
        return false;
      // 16-bit constant: LD rr,nn (pseudo, expanded after RA)
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::LD_rr_nn), DstReg)
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
    // Load address of global variable, displaced by the constant the
    // combiner folded into it.
    Register DstReg = MI.getOperand(0).getReg();
    const MachineOperand &GVOp = MI.getOperand(1);

    // Constrain destination to 16-bit register class
    if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI))
      return false;

    // Use LD_r16_nn pseudo with the global's address
    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::LD_rr_nn), DstReg)
        .addGlobalAddress(GVOp.getGlobal(), GVOp.getOffset());
    MI.eraseFromParent();
    return true;
  }

  case TargetOpcode::G_BLOCK_ADDR: {
    // Load address of a basic block (for computed goto)
    Register DstReg = MI.getOperand(0).getReg();
    const BlockAddress *BA = MI.getOperand(1).getBlockAddress();

    if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI))
      return false;

    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::LD_rr_nn), DstReg)
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

    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::LD_rr_nn), DstReg)
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

    // Port I/O: address_space(2) -> IN A,(n)
    if (MI.hasOneMemOperand() &&
        (*MI.memoperands_begin())->getAddrSpace() == Z80::AS_IO) {
      if (DstTy.getSizeInBits() > 8)
        return false; // Only 8-bit port reads supported
      // Extract constant port address from G_INTTOPTR(G_CONSTANT n)
      MachineInstr *AddrDef = MRI.getVRegDef(AddrReg);
      int64_t PortAddr = -1;
      if (AddrDef && AddrDef->getOpcode() == TargetOpcode::G_INTTOPTR) {
        Register SrcReg = AddrDef->getOperand(1).getReg();
        MachineInstr *SrcDef = MRI.getVRegDef(SrcReg);
        if (SrcDef && SrcDef->getOpcode() == TargetOpcode::G_CONSTANT)
          PortAddr = SrcDef->getOperand(1).getCImm()->getZExtValue() & 0xFF;
      } else if (AddrDef && AddrDef->getOpcode() == TargetOpcode::G_CONSTANT) {
        // The combiner folded G_INTTOPTR(G_CONSTANT n) into a pointer-typed
        // constant (e.g. port 0 shows up as `ptr addrspace(2) null`).
        PortAddr = AddrDef->getOperand(1).getCImm()->getZExtValue() & 0xFF;
      }
      // Only compile-time constant ports are supported (selects IN A,(n)).
      // Runtime-selected ports are intentionally rejected: on Z80, IN r,(C)
      // places register B on the upper 8 address lines (A8-A15). On systems
      // with 16-bit port decoding, emitting (C) without explicit control over
      // B risks unintended bus contention or misaddressing.
      if (PortAddr < 0)
        return false;
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI))
        return false;
      BuildMI(MBB, MI, DL, TII.get(Z80::IN_A_n)).addImm(PortAddr);
      BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg).addReg(Z80::A);
      MI.eraseFromParent();
      return true;
    }

    // A compile-time address needs no pointer in a register: SM83 reaches the
    // high page 0xFF00-0xFFFF in two bytes and anywhere else in three, against
    // four for loading a pair and going indirect.
    if (DstTy.getSizeInBits() == 8 && MI.hasOneMemOperand() &&
        MBB.getParent()->getSubtarget<Z80Subtarget>().hasSM83()) {
      if (std::optional<uint16_t> Addr = getConstantAddr(AddrReg, MRI)) {
        if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI))
          return false;
        bool HighPage = *Addr >= 0xFF00;
        unsigned Opc = HighPage ? Z80::SM83_LDH_Ac_nind : Z80::SM83_LD_Ac_nnind;
        buildAccOp(MBB, MI, DL, Opc, DstReg, Register(), MRI)
            .addImm(HighPage ? (*Addr & 0xFF) : *Addr)
            .cloneMemRefs(MI);
        MI.eraseFromParent();
        return true;
      }
    }

    // A pair read from an address the linker settles takes one instruction,
    // against putting the address in a pointer register and reading the two
    // bytes through it. SM83 has no such instruction.
    if (DstTy.getSizeInBits() == 16 && MI.hasOneMemOperand() &&
        !MBB.getParent()->getSubtarget<Z80Subtarget>().hasSM83()) {
      const GlobalValue *GV = nullptr;
      int64_t Offset = 0;
      if (getGlobalAddr(AddrReg, MRI, GV, Offset)) {
        if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI))
          return false;
        BuildMI(MBB, MI, DL, TII.get(Z80::LOAD16_ABS), DstReg)
            .addGlobalAddress(GV, Offset)
            .cloneMemRefs(MI);
        MI.eraseFromParent();
        return true;
      }
    }

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
          // 8-bit IX-indexed load: LD r,(IX+d), into any register.
          if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI))
            return false;
          BuildMI(MBB, MI, DL, TII.get(Z80::LD_r_IXd), DstReg).addImm(Disp);
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
          Register LdLoReg = Z80::L;
          Register LdHiReg = Z80::H;

          if (MRI.hasOneNonDBGUse(DstReg)) {
            MachineInstr &Use = *MRI.use_nodbg_begin(DstReg)->getParent();
            if (Use.getOpcode() == TargetOpcode::COPY &&
                Use.getOperand(0).getReg().isPhysical()) {
              Register PhysDst = Use.getOperand(0).getReg();
              if (PhysDst == Z80::DE) {
                TargetPair = Z80::DE;
                LdLoReg = Z80::E;
                LdHiReg = Z80::D;
              } else if (PhysDst == Z80::BC) {
                TargetPair = Z80::BC;
                LdLoReg = Z80::C;
                LdHiReg = Z80::B;
              }
            }
          }

          Z80::buildLoadIdx(MBB, MI, DL, TII, Z80::LD_r_IXd, LdLoReg, Disp)
              .addReg(TargetPair, RegState::ImplicitDefine);
          Z80::buildLoadIdx(MBB, MI, DL, TII, Z80::LD_r_IXd, LdHiReg, Disp + 1)
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
              .addImm(Disp)
              .cloneMemRefs(MI);
          MI.eraseFromParent();
          return true;
        }
        if (DstTy.getSizeInBits() <= 16) {
          if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI))
            return false;
          BuildMI(MBB, MI, DL, TII.get(Z80::RELOAD_GR16), DstReg)
              .addFrameIndex(FI)
              .addImm(Disp)
              .cloneMemRefs(MI);
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
            .addImm(0)
            .cloneMemRefs(MI);
        MI.eraseFromParent();
        return true;
      }
      if (DstTy.getSizeInBits() <= 16) {
        if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI))
          return false;
        BuildMI(MBB, MI, DL, TII.get(Z80::RELOAD_GR16), DstReg)
            .addFrameIndex(FI)
            .addImm(0)
            .cloneMemRefs(MI);
        MI.eraseFromParent();
        return true;
      }
    }

    // Fallback: indirect addressing via BC, DE, or HL.
    // LOAD8_IND accepts any GR16 register, so regalloc can choose BC/DE/HL
    // freely. This avoids forcing the address into HL, reducing register
    // pressure (LD A,(BC) and LD A,(DE) are valid Z80/SM83 instructions).
    if (DstTy.getSizeInBits() <= 8) {
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI) ||
          !RBI.constrainGenericRegister(AddrReg, Z80::GR16RegClass, MRI))
        return false;

      buildAccOp(MBB, MI, DL, Z80::LOAD8_IND, DstReg, Register(), MRI)
          .addReg(AddrReg);
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
      Z80::buildLoadHL(MBB, MI, DL, TII, Z80::E);
      // Increment address
      Z80::buildIncDec16(MBB, MI, DL, TII, Z80::INC_rr, Z80::HL);
      // Load high byte
      Z80::buildLoadHL(MBB, MI, DL, TII, Z80::D);
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

    // Port I/O: address_space(2) -> OUT (n),A
    if (MI.hasOneMemOperand() &&
        (*MI.memoperands_begin())->getAddrSpace() == Z80::AS_IO) {
      if (SrcTy.getSizeInBits() > 8)
        return false; // Only 8-bit port writes supported
      MachineInstr *AddrDef = MRI.getVRegDef(AddrReg);
      int64_t PortAddr = -1;
      if (AddrDef && AddrDef->getOpcode() == TargetOpcode::G_INTTOPTR) {
        Register IntReg = AddrDef->getOperand(1).getReg();
        MachineInstr *IntDef = MRI.getVRegDef(IntReg);
        if (IntDef && IntDef->getOpcode() == TargetOpcode::G_CONSTANT)
          PortAddr = IntDef->getOperand(1).getCImm()->getZExtValue() & 0xFF;
      } else if (AddrDef && AddrDef->getOpcode() == TargetOpcode::G_CONSTANT) {
        // The combiner folded G_INTTOPTR(G_CONSTANT n) into a pointer-typed
        // constant (e.g. port 0 shows up as `ptr addrspace(2) null`).
        PortAddr = AddrDef->getOperand(1).getCImm()->getZExtValue() & 0xFF;
      }
      // Only compile-time constant ports are supported (selects OUT (n),A).
      // Runtime-selected ports are intentionally rejected: on Z80, OUT (C),r
      // places register B on the upper 8 address lines (A8-A15). On systems
      // with 16-bit port decoding, emitting (C) without explicit control over
      // B risks unintended bus contention or misaddressing.
      if (PortAddr < 0)
        return false;
      if (!RBI.constrainGenericRegister(SrcReg, Z80::GR8RegClass, MRI))
        return false;
      BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::A).addReg(SrcReg);
      BuildMI(MBB, MI, DL, TII.get(Z80::OUT_n_A)).addImm(PortAddr);
      MI.eraseFromParent();
      return true;
    }

    // See the matching fold in G_LOAD.
    if (SrcTy.getSizeInBits() == 8 && MI.hasOneMemOperand() &&
        MBB.getParent()->getSubtarget<Z80Subtarget>().hasSM83()) {
      if (std::optional<uint16_t> Addr = getConstantAddr(AddrReg, MRI)) {
        if (!RBI.constrainGenericRegister(SrcReg, Z80::GR8RegClass, MRI))
          return false;
        bool HighPage = *Addr >= 0xFF00;
        unsigned Opc = HighPage ? Z80::SM83_LDH_nind_Ac : Z80::SM83_LD_nnind_Ac;
        buildAccOp(MBB, MI, DL, Opc, Register(), SrcReg, MRI)
            .addImm(HighPage ? (*Addr & 0xFF) : *Addr)
            .cloneMemRefs(MI);
        MI.eraseFromParent();
        return true;
      }
    }

    // See the matching fold in G_LOAD.
    if (SrcTy.getSizeInBits() == 16 && MI.hasOneMemOperand() &&
        !MBB.getParent()->getSubtarget<Z80Subtarget>().hasSM83()) {
      const GlobalValue *GV = nullptr;
      int64_t Offset = 0;
      if (getGlobalAddr(AddrReg, MRI, GV, Offset)) {
        if (!RBI.constrainGenericRegister(SrcReg, Z80::GR16RegClass, MRI))
          return false;
        BuildMI(MBB, MI, DL, TII.get(Z80::STORE16_ABS))
            .addGlobalAddress(GV, Offset)
            .addReg(SrcReg)
            .cloneMemRefs(MI);
        MI.eraseFromParent();
        return true;
      }
    }

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
                .addImm(ExtraOffset)
                .cloneMemRefs(MI);
            MI.eraseFromParent();
            return true;
          }
          if (!RBI.constrainGenericRegister(SrcReg, Z80::GR8RegClass, MRI))
            return false;
          BuildMI(MBB, MI, DL, TII.get(Z80::SPILL_GR8))
              .addReg(SrcReg)
              .addFrameIndex(FI)
              .addImm(ExtraOffset)
              .cloneMemRefs(MI);
          MI.eraseFromParent();
          return true;
        }
        if (SrcTy.getSizeInBits() <= 16) {
          // As above for 8 bits: a constant goes straight to memory rather
          // than through a register pair, which also keeps the pair free.
          // A second use of the constant pays for the pair on Z80, which is
          // a byte saved for a few cycles spent and so the size levels'
          // trade. SM83 pays for its own address setup on every frame
          // access and gets nothing back from the pair.
          const MachineFunction &StoreMF = *MBB.getParent();
          bool KeepSharedPair =
              !StoreMF.getSubtarget<Z80Subtarget>().hasSM83() &&
              StoreMF.getFunction().hasOptSize();
          MachineInstr *SrcDef = MRI.getVRegDef(SrcReg);
          if (SrcDef && SrcDef->getOpcode() == TargetOpcode::G_CONSTANT &&
              (!KeepSharedPair || MRI.hasOneNonDBGUse(SrcReg))) {
            int64_t Val =
                SrcDef->getOperand(1).getCImm()->getSExtValue() & 0xFFFF;
            BuildMI(MBB, MI, DL, TII.get(Z80::SPILL_IMM16))
                .addImm(Val)
                .addFrameIndex(FI)
                .addImm(ExtraOffset)
                .cloneMemRefs(MI);
            MI.eraseFromParent();
            return true;
          }
          if (!RBI.constrainGenericRegister(SrcReg, Z80::GR16RegClass, MRI))
            return false;
          BuildMI(MBB, MI, DL, TII.get(Z80::SPILL_GR16))
              .addReg(SrcReg)
              .addFrameIndex(FI)
              .addImm(ExtraOffset)
              .cloneMemRefs(MI);
          MI.eraseFromParent();
          return true;
        }
      }
    }

    if (SrcTy.getSizeInBits() <= 8) {
      // 8-bit store via indirect addressing (BC, DE, or HL).
      if (!RBI.constrainGenericRegister(SrcReg, Z80::GR8RegClass, MRI) ||
          !RBI.constrainGenericRegister(AddrReg, Z80::GR16RegClass, MRI))
        return false;

      buildAccOp(MBB, MI, DL, Z80::STORE8_IND, Register(), SrcReg, MRI)
          .addReg(AddrReg);
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
      bool IsUndef =
          SrcDef && SrcDef->getOpcode() == TargetOpcode::G_IMPLICIT_DEF;

      if (!IsUndef)
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::DE)
            .addReg(SrcReg);
      // Copy address to HL
      BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::HL)
          .addReg(AddrReg);
      // Store low byte directly from E (no A intermediary)
      auto &StoreLo = *Z80::buildStoreHL(MBB, MI, DL, TII, Z80::E);
      // Increment address
      Z80::buildIncDec16(MBB, MI, DL, TII, Z80::INC_rr, Z80::HL);
      // Store high byte directly from D
      auto &StoreHi = *Z80::buildStoreHL(MBB, MI, DL, TII, Z80::D);
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

    // Check for small constant offset: repeated INC rr/DEC rr (1 byte each)
    // is smaller than LD rr,nn + ADD HL,rr (4 bytes) for |offset| <= 3.
    MachineInstr *OffDef = MRI.getVRegDef(OffReg);
    if (OffDef && OffDef->getOpcode() == TargetOpcode::G_CONSTANT) {
      int64_t OffVal = OffDef->getOperand(1).getCImm()->getSExtValue();
      if (OffVal != 0 && std::abs(OffVal) <= 3) {
        if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI) ||
            !RBI.constrainGenericRegister(BaseReg, Z80::GR16RegClass, MRI))
          return false;
        unsigned StepOpc = (OffVal > 0) ? Z80::INC_rr : Z80::DEC_rr;
        int64_t Count = std::abs(OffVal);
        Register PrevReg = BaseReg;
        for (int64_t i = 0; i < Count; i++) {
          Register OutReg = (i == Count - 1)
                                ? DstReg
                                : MRI.createVirtualRegister(&Z80::GR16RegClass);
          BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(StepOpc), OutReg)
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

        // INC and DEC work on any register, so the allocator decides
        // whether the value steps where it lives or in A with its neighbours.
        if (Val == 1 || (Val & 0xFF) == 0xFF)
          BuildMI(MBB, MI, MI.getDebugLoc(),
                  TII.get(Val == 1 ? Z80::INC_r : Z80::DEC_r), DstReg)
              .addReg(Src1Reg);
        else
          buildAccOp(MBB, MI, MI.getDebugLoc(), Z80::ADD_Ac_n, DstReg, Src1Reg,
                     MRI)
              .addImm(Val & 0xFF);
        MI.eraseFromParent();
        return true;
      }

      if (tryAluFIFold(Src1Reg, Src2Reg, DstReg, Z80::ALU_ADD) ||
          tryAluFIFold(Src2Reg, Src1Reg, DstReg, Z80::ALU_ADD))
        return true;

      // Constrain registers
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI) ||
          !RBI.constrainGenericRegister(Src1Reg, Z80::GR8RegClass, MRI) ||
          !RBI.constrainGenericRegister(Src2Reg, Z80::GR8RegClass, MRI))
        return false;

      buildAccOp(MBB, MI, MI.getDebugLoc(), Z80::ADD_Ac_r, DstReg, Src1Reg, MRI)
          .addReg(Src2Reg);
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
      // Check for constant +1/-1 to use INC rr/DEC rr
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
        // Check for small constants that can use repeated INC rr/DEC rr.
        // INC rr is 1 byte each, vs LD rr,nn (3 bytes) + ADD HL,rr (1 byte).
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
          unsigned StepOpc = (Imm > 0) ? Z80::INC_rr : Z80::DEC_rr;
          int64_t Count = std::abs(Imm);
          Register PrevReg = SrcReg;
          for (int64_t i = 0; i < Count; i++) {
            Register OutReg =
                (i == Count - 1)
                    ? DstReg
                    : MRI.createVirtualRegister(&Z80::GR16RegClass);
            BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(StepOpc), OutReg)
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

        if (Val == 1 || (Val & 0xFF) == 0xFF)
          BuildMI(MBB, MI, MI.getDebugLoc(),
                  TII.get(Val == 1 ? Z80::DEC_r : Z80::INC_r), DstReg)
              .addReg(Src1Reg);
        else
          buildAccOp(MBB, MI, MI.getDebugLoc(), Z80::SUB_Ac_n, DstReg, Src1Reg,
                     MRI)
              .addImm(Val & 0xFF);
        MI.eraseFromParent();
        return true;
      }

      if (tryAluFIFold(Src1Reg, Src2Reg, DstReg, Z80::ALU_SUB))
        return true;

      // Constrain registers
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI) ||
          !RBI.constrainGenericRegister(Src1Reg, Z80::GR8RegClass, MRI) ||
          !RBI.constrainGenericRegister(Src2Reg, Z80::GR8RegClass, MRI))
        return false;

      buildAccOp(MBB, MI, MI.getDebugLoc(), Z80::SUB_Ac_r, DstReg, Src1Reg, MRI)
          .addReg(Src2Reg);
      MI.eraseFromParent();
      return true;
    }

    if (DstTy.getSizeInBits() <= 16) {
      // Constrain registers
      if (!RBI.constrainGenericRegister(DstReg, Z80::GR16RegClass, MRI) ||
          !RBI.constrainGenericRegister(Src1Reg, Z80::GR16RegClass, MRI) ||
          !RBI.constrainGenericRegister(Src2Reg, Z80::GR16RegClass, MRI))
        return false;

      // Check for small constant to use repeated DEC rr/INC rr.
      // SUB N → DEC rr × N, SUB -N → INC rr × N. Worth it for |N| <= 3.
      {
        MachineInstr *Src2Def = MRI.getVRegDef(Src2Reg);
        if (Src2Def && Src2Def->getOpcode() == TargetOpcode::G_CONSTANT) {
          int64_t Val = Src2Def->getOperand(1).getCImm()->getSExtValue();
          if (Val != 0 && std::abs(Val) <= 3) {
            unsigned StepOpc = (Val > 0) ? Z80::DEC_rr : Z80::INC_rr;
            int64_t Count = std::abs(Val);
            Register PrevReg = Src1Reg;
            for (int64_t i = 0; i < Count; i++) {
              Register OutReg =
                  (i == Count - 1)
                      ? DstReg
                      : MRI.createVirtualRegister(&Z80::GR16RegClass);
              BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(StepOpc), OutReg)
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

      // Masking with 1 a value selection builds as exactly 0 or 1 keeps it
      // as it is.
      auto isMaskedBool = [&](Register Val, Register Mask) {
        auto M = getConst8(Mask);
        return M && (*M & 0xFF) == 1 && isSelectedBool(Val, MRI);
      };

      if (isAllOnesConst(Src2Reg) || isMaskedBool(Src1Reg, Src2Reg)) {
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Src1Reg);
      } else if (isAllOnesConst(Src1Reg) || isMaskedBool(Src2Reg, Src1Reg)) {
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
            .addReg(Src2Reg);
      } else if (isZeroConst8(Src1Reg) || isZeroConst8(Src2Reg)) {
        // AND with 0 → load immediate 0
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::LD_r_n), DstReg)
            .addImm(0);
      } else {
        // Try immediate fold: AND with constant → AND_n
        // Commute: put constant in Src2
        if (getConst8(Src1Reg) && !getConst8(Src2Reg))
          std::swap(Src1Reg, Src2Reg);
        auto ImmVal = getConst8(Src2Reg);

        // Clearing one bit is RES, which works in any register and leaves the
        // flags alone; a test that wants them turns it back into AND.
        if (ImmVal && isPowerOf2_32(~*ImmVal & 0xFF)) {
          BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::RES_b_r), DstReg)
              .addImm(Log2_32(~*ImmVal & 0xFF))
              .addReg(Src1Reg);
          MI.eraseFromParent();
          return true;
        }

        if (!ImmVal && (tryAluFIFold(Src1Reg, Src2Reg, DstReg, Z80::ALU_AND) ||
                        tryAluFIFold(Src2Reg, Src1Reg, DstReg, Z80::ALU_AND)))
          return true;

        if (ImmVal)
          buildAccOp(MBB, MI, MI.getDebugLoc(), Z80::AND_Ac_n, DstReg, Src1Reg,
                     MRI)
              .addImm(*ImmVal & 0xFF);
        else
          buildAccOp(MBB, MI, MI.getDebugLoc(), Z80::AND_Ac_r, DstReg, Src1Reg,
                     MRI)
              .addReg(Src2Reg);
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
      unsigned AluOp =
          Opcode == TargetOpcode::G_OR ? Z80::ALU_OR : Z80::ALU_XOR;
      if (tryAluFIFold(Src1Reg, Src2Reg, DstReg, AluOp) ||
          tryAluFIFold(Src2Reg, Src1Reg, DstReg, AluOp))
        return true;

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

        bool IsOr = Opcode == TargetOpcode::G_OR;
        // Setting one bit is SET, as clearing one is RES, and flipping all
        // of them is CPL. None of them writes the flags OR and XOR would; a
        // test that wants those turns them back into OR and XOR.
        if (IsOr && ImmVal && isPowerOf2_32(*ImmVal & 0xFF))
          BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::SET_b_r), DstReg)
              .addImm(Log2_32(*ImmVal & 0xFF))
              .addReg(Src1Reg);
        else if (!IsOr && ImmVal && (*ImmVal & 0xFF) == 0xFF)
          buildAccOp(MBB, MI, MI.getDebugLoc(), Z80::CPL_Ac, DstReg, Src1Reg,
                     MRI);
        else if (ImmVal)
          buildAccOp(MBB, MI, MI.getDebugLoc(),
                     IsOr ? Z80::OR_Ac_n : Z80::XOR_Ac_n, DstReg, Src1Reg, MRI)
              .addImm(*ImmVal & 0xFF);
        else
          buildAccOp(MBB, MI, MI.getDebugLoc(),
                     IsOr ? Z80::OR_Ac_r : Z80::XOR_Ac_r, DstReg, Src1Reg, MRI)
              .addReg(Src2Reg);
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

      Register Result;
      if (ShiftAmt >= 0) {
        Result = emitShlByte(MBB, MI, DL, SrcReg, ShiftAmt, MRI);
      } else {
        // Variable shift: a DJNZ loop, with the count in B.
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::B)
            .addReg(ShiftAmtReg);
        Result = MRI.createVirtualRegister(&Z80::GR8RegClass);
        BuildMI(MBB, MI, DL, TII.get(Z80::SHL8_VAR), Result).addReg(SrcReg);
      }
      BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg).addReg(Result);
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
        BuildMI(MBB, MI, DL, TII.get(Z80::LD_rr_nn), DstReg).addImm(0);
      } else if (ShiftAmt >= 8) {
        // The low byte moves up and shifts on its own, into the high half of
        // a pair whose low half is zero. A zero-extended byte is taken as it
        // is.
        Register Byte;
        MachineInstr *SrcDef = MRI.getVRegDef(SrcReg);
        if (SrcDef && SrcDef->getOpcode() == TargetOpcode::G_ZEXT) {
          Byte = SrcDef->getOperand(1).getReg();
          if (!RBI.constrainGenericRegister(Byte, Z80::GR8RegClass, MRI))
            return false;
        } else {
          Byte = MRI.createVirtualRegister(&Z80::GR8RegClass);
          BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Byte)
              .addReg(SrcReg, RegState{}, Z80::sub_lo);
        }
        Register Hi = emitShlByte(MBB, MI, DL, Byte, ShiftAmt - 8, MRI);
        BuildMI(MBB, MI, DL, TII.get(Z80::SHL8_GR8_GR16), DstReg).addReg(Hi);
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

      Register Result;
      if (ShiftAmt >= 0) {
        Result = emitLshrByte(MBB, MI, DL, SrcReg, ShiftAmt, MRI);
      } else {
        // Variable shift: a DJNZ loop, with the count in B.
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::B)
            .addReg(ShiftAmtReg);
        Result = MRI.createVirtualRegister(&Z80::GR8RegClass);
        BuildMI(MBB, MI, DL, TII.get(Z80::LSHR8_VAR), Result).addReg(SrcReg);
      }
      BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg).addReg(Result);
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
        BuildMI(MBB, MI, DL, TII.get(Z80::LD_rr_nn), DstReg).addImm(0);
      } else if (ShiftAmt >= 8) {
        // The high byte moves down, shifts on its own and is zero-extended.
        Register Byte = MRI.createVirtualRegister(&Z80::GR8RegClass);
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Byte)
            .addReg(SrcReg, RegState{}, Z80::sub_hi);
        Register Lo = emitLshrByte(MBB, MI, DL, Byte, ShiftAmt - 8, MRI);
        BuildMI(MBB, MI, DL, TII.get(Z80::ZEXT_GR8_GR16), DstReg).addReg(Lo);
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

      Register Result;
      if (ShiftAmt >= 0) {
        Result = emitAshrByte(MBB, MI, DL, SrcReg, ShiftAmt, MRI);
      } else {
        // Variable shift: a DJNZ loop, with the count in B.
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::B)
            .addReg(ShiftAmtReg);
        Result = MRI.createVirtualRegister(&Z80::GR8RegClass);
        BuildMI(MBB, MI, DL, TII.get(Z80::ASHR8_VAR), Result).addReg(SrcReg);
      }
      BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg).addReg(Result);
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
        // The high byte moves down, shifts on its own and is sign-extended.
        Register Byte = MRI.createVirtualRegister(&Z80::GR8RegClass);
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Byte)
            .addReg(SrcReg, RegState{}, Z80::sub_hi);
        Register Lo = emitAshrByte(MBB, MI, DL, Byte, ShiftAmt - 8, MRI);
        BuildMI(MBB, MI, DL, TII.get(Z80::SEXT_GR8_GR16), DstReg).addReg(Lo);
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

    Register Result = SrcReg;
    if (Amt == 4 && STI.hasSM83()) {
      // SM83: SWAP A is a single-instruction nibble swap (rotate by 4).
      Result = emitAccUnary(MBB, MI, DL, Z80::SWAP_Ac, SrcReg, MRI);
    } else if (Amt > 0) {
      // For amounts > 4, rotate the other direction (fewer instructions).
      // E.g. ROTL by 6 = ROTR by 2 (2 instructions instead of 6).
      unsigned Opc;
      int64_t Count;
      if (Amt <= 4) {
        Opc = IsLeft ? Z80::RLCA_Ac : Z80::RRCA_Ac;
        Count = Amt;
      } else {
        Opc = IsLeft ? Z80::RRCA_Ac : Z80::RLCA_Ac;
        Count = 8 - Amt;
      }
      for (int64_t i = 0; i < Count; i++)
        Result = emitAccUnary(MBB, MI, DL, Opc, Result, MRI);
    } else if (Amt < 0) {
      // Variable rotation: a DJNZ loop, with the count in B.
      BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::B).addReg(AmtReg);
      Result = emitAccUnary(
          MBB, MI, DL, IsLeft ? Z80::ROTL8_VAR : Z80::ROTR8_VAR, SrcReg, MRI);
    }
    BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg).addReg(Result);
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

    Register Bool;
    if (LHSTy.getSizeInBits() <= 8) {
      // 8-bit comparison using generalized ALU pseudos.
      // Check if RHS is a constant for immediate-form instructions.
      auto getRHSConst = [&](Register Reg) -> std::optional<int64_t> {
        MachineInstr *Def = MRI.getVRegDef(Reg);
        if (Def && Def->getOpcode() == TargetOpcode::G_CONSTANT)
          return Def->getOperand(1).getCImm()->getSExtValue();
        return std::nullopt;
      };

      // CP sets the carry when its left operand is below its right one.
      auto emitCP = [&](Register L, Register R) {
        if (auto C = getRHSConst(R))
          buildAccOp(MBB, MI, MI.getDebugLoc(), Z80::CP_Ac_n, Register(), L,
                     MRI)
              .addImm(*C & 0xFF);
        else
          buildAccOp(MBB, MI, MI.getDebugLoc(), Z80::CP_Ac_r, Register(), L,
                     MRI)
              .addReg(R);
      };

      if (!RBI.constrainGenericRegister(LHS, Z80::GR8RegClass, MRI) ||
          !RBI.constrainGenericRegister(RHS, Z80::GR8RegClass, MRI))
        return false;

      switch (Pred) {
      case CmpInst::ICMP_EQ:
      case CmpInst::ICMP_NE: {
        std::optional<Register> Diff =
            emitEqualityTest(MBB, MI, MI.getDebugLoc(), LHS, RHS,
                             /*FlagsOnly=*/false, MRI);
        if (!Diff)
          return false;
        Bool = emitBoolFromZero(MBB, MI, MI.getDebugLoc(), *Diff,
                                Pred == CmpInst::ICMP_EQ, MRI);
        break;
      }

      // UGT and ULE are ULT and UGE with the operands swapped.
      case CmpInst::ICMP_ULT:
      case CmpInst::ICMP_UGE:
        emitCP(LHS, RHS);
        Bool = emitBoolFromCarry(MBB, MI, MI.getDebugLoc(),
                                 Pred == CmpInst::ICMP_ULT, MRI);
        break;
      case CmpInst::ICMP_UGT:
      case CmpInst::ICMP_ULE:
        emitCP(RHS, LHS);
        Bool = emitBoolFromCarry(MBB, MI, MI.getDebugLoc(),
                                 Pred == CmpInst::ICMP_UGT, MRI);
        break;

      case CmpInst::ICMP_SLT:
      case CmpInst::ICMP_SGE: {
        // The legalizer leaves a signed order only as x < 0 or x >= 0: the sign
        // bit, rotated down to bit 0.
        std::optional<int64_t> RC = getRHSConst(RHS);
        if (!RC || *RC != 0)
          return false;
        const DebugLoc &DL = MI.getDebugLoc();
        Register Sign = Pred == CmpInst::ICMP_SGE
                            ? emitAccUnary(MBB, MI, DL, Z80::CPL_Ac, LHS, MRI)
                            : LHS;
        Bool = emitLshrByte(MBB, MI, DL, Sign, 7, MRI);
        break;
      }

      default:
        return false;
      }
    } else if (LHSTy.getSizeInBits() <= 16) {
      DebugLoc DL = MI.getDebugLoc();

      if (Pred == CmpInst::ICMP_EQ || Pred == CmpInst::ICMP_NE) {
        std::optional<Register> Diff =
            emitEqualityTest(MBB, MI, DL, LHS, RHS, /*FlagsOnly=*/false, MRI);
        if (!Diff)
          return false;
        Bool =
            emitBoolFromZero(MBB, MI, DL, *Diff, Pred == CmpInst::ICMP_EQ, MRI);
      } else if (ICmpInst::isSigned(Pred)) {
        // The legalizer leaves no signed order on pairs.
        return false;
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

        if (Pred != CmpInst::ICMP_ULT && Pred != CmpInst::ICMP_UGE)
          return false;
        if (!emitOrderChain(MBB, MI, DL, LHS, RHS, MRI))
          return false;
        Bool = emitBoolFromCarry(MBB, MI, DL, Pred == CmpInst::ICMP_ULT, MRI);
      }
    } else {
      return false;
    }

    BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
        .addReg(Bool);

    MI.eraseFromParent();
    return true;
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
    Register Bool;
    if (!emit32CompareFlags(MBB, MI, Pred, LhsLo, LhsHi, RhsLo, RhsHi, MRI, DL,
                            NormPred, &Bool))
      return false;

    // An order leaves its answer in the carry, set if LHS < RHS.
    if (NormPred != CmpInst::ICMP_EQ && NormPred != CmpInst::ICMP_NE)
      Bool = emitBoolFromCarry(MBB, MI, DL, NormPred == CmpInst::ICMP_ULT, MRI);

    if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI))
      return false;
    BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg).addReg(Bool);

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
    Register Bool;
    if (!emit64CompareFlags(MBB, MI, Pred, LhsW0, LhsW1, LhsW2, LhsW3, RhsW0,
                            RhsW1, RhsW2, RhsW3, MRI, DL, NormPred, &Bool))
      return false;

    // An order leaves its answer in the carry, as for G_Z80_ICMP32.
    if (NormPred != CmpInst::ICMP_EQ && NormPred != CmpInst::ICMP_NE)
      Bool = emitBoolFromCarry(MBB, MI, DL, NormPred == CmpInst::ICMP_ULT, MRI);

    if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI))
      return false;
    BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg).addReg(Bool);

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
                               MI.getDebugLoc(), NormPred)) {
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
                               NormPred)) {
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

    buildAccOp(MBB, MI, MI.getDebugLoc(), Z80::TST_Ac, Register(), CondReg,
               MRI);
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
              BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::E)
                  .addReg(Orig2);
              buildAccOp(MBB, MI, DL, IsSDiv ? Z80::SDIV8 : Z80::SMOD8, DstReg,
                         Orig1, MRI);
            }

            SrcDef->eraseFromParent(); // erase G_SDIV/G_SREM
            MI.eraseFromParent();      // erase G_TRUNC
            return true;
          }
        }
      }

      // Fold trunc(ptrtoint(global)) and trunc(lshr(ptrtoint(global), 8))
      // into an 8-bit immediate load of the symbol's low/high address byte.
      // The GB banking convention encodes a bank number as a symbol's
      // link-time address, so only one byte of it is wanted; materializing
      // the full 16-bit address costs twice the bytes and burns a register
      // pair. The Addr16_Low/High fixups carry the byte to the linker.
      if (SrcDef && MRI.hasOneNonDBGUse(SrcReg)) {
        // The truncated byte: low by default; high through lshr by 8.
        MachineInstr *AddrDef = SrcDef;
        unsigned Flag = Z80::MO_ADDR16_LO;
        if (SrcDef->getOpcode() == TargetOpcode::G_LSHR) {
          auto ShAmt = getIConstantVRegValWithLookThrough(
              SrcDef->getOperand(2).getReg(), MRI);
          Register ShSrc = SrcDef->getOperand(1).getReg();
          if (ShAmt && ShAmt->Value == 8 && MRI.hasOneNonDBGUse(ShSrc)) {
            AddrDef = MRI.getVRegDef(ShSrc);
            Flag = Z80::MO_ADDR16_HI;
          }
        }
        MachineInstr *GlobalDef =
            AddrDef && AddrDef->getOpcode() == TargetOpcode::G_PTRTOINT
                ? MRI.getVRegDef(AddrDef->getOperand(1).getReg())
                : nullptr;
        if (GlobalDef &&
            GlobalDef->getOpcode() == TargetOpcode::G_GLOBAL_VALUE) {
          if (!RBI.constrainGenericRegister(DstReg, Z80::GR8RegClass, MRI))
            return false;
          const MachineOperand &GVOp = GlobalDef->getOperand(1);
          BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::LD_r_n), DstReg)
              .addGlobalAddress(GVOp.getGlobal(), GVOp.getOffset(), Flag);
          MI.eraseFromParent(); // erase G_TRUNC
          // The address chain (G_LSHR, G_PTRTOINT, G_GLOBAL_VALUE, shift
          // amount) may have other users; whatever is now dead is removed
          // by the selector's trivially-dead sweep.
          return true;
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

    const DebugLoc &DL = MI.getDebugLoc();
    // The low byte is the source itself where that is already a byte, and
    // otherwise the low half of the pair the source occupies. Naming the half
    // in place leaves the value wherever it is: moving it into a fixed pair
    // first would tie the allocator's hands and leave a definition of that
    // pair which the value outlives.
    Register LowByte = SrcReg;
    unsigned LowSubReg = 0;

    if (SrcBits <= 8) {
      if (!RBI.constrainGenericRegister(SrcReg, Z80::GR8RegClass, MRI))
        return false;
    } else {
      if (!RBI.constrainGenericRegister(SrcReg, Z80::GR16RegClass, MRI))
        return false;
      LowSubReg = Z80::sub_lo;
    }

    if (DstBits == 1) {
      // The rest of the backend reads an s1 in a GR8 as exactly 0 or 1.
      // Truncation is the one producer that can leave other bits set.
      Register Byte = LowByte;
      if (LowSubReg) {
        Byte = MRI.createVirtualRegister(&Z80::GR8RegClass);
        BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Byte)
            .addReg(LowByte, RegState{}, LowSubReg);
      }
      LowByte = emitAccImm(MBB, MI, DL, Z80::AND_Ac_n, Byte, 1, MRI);
      LowSubReg = 0;
    }

    BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), DstReg)
        .addReg(LowByte, RegState{}, LowSubReg);
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
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::ADD_HL_rr))
          .addReg(Src1Reg);
      if (!emitCarryOut(MBB, MI, OverflowReg, MRI))
        return false;
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
          .addReg(Z80::HL);
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

      bool OverflowUsed = !MRI.use_nodbg_empty(OverflowReg);
      if (OverflowUsed &&
          !RBI.constrainGenericRegister(OverflowReg, Z80::GR8RegClass, MRI))
        return false;
      Register Overflow = OverflowUsed ? OverflowReg : Register();

      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::HL)
          .addReg(Src1Reg);

      if (STI.hasSM83()) {
        // SM83: combined add + overflow detection (no P/V flag), the
        // overflow coming out in A.
        buildAccOp(MBB, MI, MI.getDebugLoc(), Z80::SM83_SADDO_HL_rr, Overflow,
                   Register(), MRI)
            .addReg(Src2Reg);
      } else {
        // Z80: AND A; ADC HL,rr — sets P/V for signed overflow.
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::SADD_HL_rr))
            .addReg(Src2Reg);
      }

      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
          .addReg(Z80::HL);

      // Z80: CAPTURE_PV reads the P/V flag (bit 2 of F) into A as 0 or 1. It
      // goes through HL, so the result leaves HL first.
      if (OverflowUsed && !STI.hasSM83())
        buildAccOp(MBB, MI, MI.getDebugLoc(), Z80::CAPTURE_PV, Overflow,
                   Register(), MRI);
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

      bool OverflowUsed = !MRI.use_nodbg_empty(OverflowReg);
      if (OverflowUsed &&
          !RBI.constrainGenericRegister(OverflowReg, Z80::GR8RegClass, MRI))
        return false;
      Register Overflow = OverflowUsed ? OverflowReg : Register();

      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::HL)
          .addReg(Src1Reg);

      if (STI.hasSM83()) {
        // SM83: combined sub + overflow detection (no P/V flag), the
        // overflow coming out in A.
        buildAccOp(MBB, MI, MI.getDebugLoc(), Z80::SM83_SSUBO_HL_rr, Overflow,
                   Register(), MRI)
            .addReg(Src2Reg);
      } else {
        // Z80: AND A; SBC HL,rr — sets P/V for signed overflow.
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::SUB_HL_rr))
            .addReg(Src2Reg);
      }

      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
          .addReg(Z80::HL);

      // Z80: CAPTURE_PV reads the P/V flag (bit 2 of F) into A as 0 or 1. It
      // goes through HL, so the result leaves HL first.
      if (OverflowUsed && !STI.hasSM83())
        buildAccOp(MBB, MI, MI.getDebugLoc(), Z80::CAPTURE_PV, Overflow,
                   Register(), MRI);
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

      // ADC_HL_rr_CI restores the carry from its register (LD A,carry;
      // RRCA) right before the ADC HL,rr that reads it.
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::HL)
          .addReg(Src1Reg);
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::ADC_HL_rr_CI))
          .addReg(Src2Reg)
          .addReg(CarryInReg);
      if (!emitCarryOut(MBB, MI, CarryOutReg, MRI))
        return false;
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
          .addReg(Z80::HL);
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
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::SUB_HL_rr))
          .addReg(Src2Reg);
      if (!emitCarryOut(MBB, MI, OverflowReg, MRI))
        return false;
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
          .addReg(Z80::HL);
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

      // SBC_HL_rr_BI restores the borrow from its register (LD A,borrow;
      // RRCA) right before the SBC HL,rr that reads it.
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::HL)
          .addReg(Src1Reg);
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::SBC_HL_rr_BI))
          .addReg(Src2Reg)
          .addReg(BorrowInReg);
      if (!emitCarryOut(MBB, MI, BorrowOutReg, MRI))
        return false;
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), DstReg)
          .addReg(Z80::HL);
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

  case TargetOpcode::G_UDIVREM:
  case TargetOpcode::G_SDIVREM: {
    // Fused divrem: one runtime call returns both quotient and remainder.
    // The ...hi3 routines promise only the quotient, so this calls the
    // ...hi4 pair, which names the remainder as a result too.
    // Z80:  __(u)divmodhi4: HL=dividend, DE=divisor → DE=quot, HL=rem
    // SM83: __(u)divmodhi4: DE=dividend, BC=divisor → BC=quot, HL=rem
    Register QuotReg = MI.getOperand(0).getReg();
    Register RemReg = MI.getOperand(1).getReg();
    Register LHSReg = MI.getOperand(2).getReg();
    Register RHSReg = MI.getOperand(3).getReg();

    if (MRI.getType(QuotReg).getSizeInBits() > 16)
      return false;

    // Inline runtime has no fused divrem pseudo — expand to separate ops.
    if (STI.inlineI16Runtime()) {
      bool IsSigned = MI.getOpcode() == TargetOpcode::G_SDIVREM;
      unsigned DivOpc = IsSigned ? Z80::SDIV16 : Z80::UDIV16;
      unsigned ModOpc = IsSigned ? Z80::SMOD16 : Z80::UMOD16;

      if (!RBI.constrainGenericRegister(QuotReg, Z80::GR16RegClass, MRI) ||
          !RBI.constrainGenericRegister(RemReg, Z80::GR16RegClass, MRI) ||
          !RBI.constrainGenericRegister(LHSReg, Z80::GR16RegClass, MRI) ||
          !RBI.constrainGenericRegister(RHSReg, Z80::GR16RegClass, MRI))
        return false;

      const DebugLoc &DL = MI.getDebugLoc();

      // Quotient
      BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::HL).addReg(LHSReg);
      BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::DE).addReg(RHSReg);
      BuildMI(MBB, MI, DL, TII.get(DivOpc));
      BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), QuotReg)
          .addReg(Z80::DE);

      // Remainder
      BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::HL).addReg(LHSReg);
      BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), Z80::DE).addReg(RHSReg);
      BuildMI(MBB, MI, DL, TII.get(ModOpc));
      BuildMI(MBB, MI, DL, TII.get(TargetOpcode::COPY), RemReg).addReg(Z80::DE);

      MI.eraseFromParent();
      return true;
    }

    if (!RBI.constrainGenericRegister(QuotReg, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(RemReg, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(LHSReg, Z80::GR16RegClass, MRI) ||
        !RBI.constrainGenericRegister(RHSReg, Z80::GR16RegClass, MRI))
      return false;

    bool IsSigned = MI.getOpcode() == TargetOpcode::G_SDIVREM;
    const char *FuncName = IsSigned ? "__divmodhi4" : "__udivmodhi4";
    Module *M = const_cast<Module *>(MF.getFunction().getParent());
    FunctionCallee Func = M->getOrInsertFunction(
        FuncName, FunctionType::get(Type::getInt16Ty(M->getContext()),
                                    {Type::getInt16Ty(M->getContext()),
                                     Type::getInt16Ty(M->getContext())},
                                    false));
    GlobalValue *GV = cast<GlobalValue>(Func.getCallee());

    if (STI.hasSM83()) {
      // SM83: DE=dividend, BC=divisor → BC=quot, HL=rem
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::DE)
          .addReg(LHSReg);
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::BC)
          .addReg(RHSReg);
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::CALL_nn))
          .addGlobalAddress(GV)
          .addUse(Z80::DE, RegState::Implicit)
          .addUse(Z80::BC, RegState::Implicit);
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), QuotReg)
          .addReg(Z80::BC);
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), RemReg)
          .addReg(Z80::HL);
    } else {
      // Z80: HL=dividend, DE=divisor → DE=quot, HL=rem
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::HL)
          .addReg(LHSReg);
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), Z80::DE)
          .addReg(RHSReg);
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::CALL_nn))
          .addGlobalAddress(GV)
          .addUse(Z80::HL, RegState::Implicit)
          .addUse(Z80::DE, RegState::Implicit);
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), QuotReg)
          .addReg(Z80::DE);
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), RemReg)
          .addReg(Z80::HL);
    }
    MI.eraseFromParent();
    return true;
  }

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

    buildAccOp(MBB, MI, MI.getDebugLoc(), PseudoOpc, DstReg, Src1Reg, MRI)
        .addReg(Src2Reg);
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

      // IN r,(C), into any register
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::IN8_C), DstReg);

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

      // OUT (C),r, from any register
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(Z80::OUT8_C)).addReg(ValueReg);

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
