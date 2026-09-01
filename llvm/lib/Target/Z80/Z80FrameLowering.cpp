//===-- Z80FrameLowering.cpp - Z80 Frame Information ----------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the Z80 implementation of TargetFrameLowering class.
//
// The Z80 stack grows downward. The stack pointer (SP) points to the last
// used stack location. PUSH decrements SP first, then stores. POP loads
// first, then increments SP.
//
//===----------------------------------------------------------------------===//

#include "Z80FrameLowering.h"

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80.h"
#include "Z80MachineFunctionInfo.h"
#include "Z80OpcodeUtils.h"
#include "Z80RegisterInfo.h"
#include "Z80Subtarget.h"

#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/Support/ErrorHandling.h"

#define DEBUG_TYPE "z80-framelowering"

using namespace llvm;

Z80FrameLowering::Z80FrameLowering()
    : TargetFrameLowering(StackGrowsDown, /*StackAlignment=*/Align(1),
                          /*LocalAreaOffset=*/-2) {}

/// PUSH AF moves SP by two. The bytes it writes become frame space that the
/// locals overwrite, so what it reads out of AF never reaches anything.
static void emitStackAllocPush(MachineBasicBlock &MBB,
                               MachineBasicBlock::iterator I, const DebugLoc &DL,
                               const TargetInstrInfo &TII) {
  MachineInstrBuilder MIB = BuildMI(MBB, I, DL, TII.get(Z80::PUSH_AF));
  for (MachineOperand &MO : MIB->operands())
    if (MO.isReg() && MO.isUse())
      MO.setIsUndef();
}

bool Z80FrameLowering::hasFPImpl(const MachineFunction &MF) const {
  const auto &STI = MF.getSubtarget<Z80Subtarget>();

  // SM83 has no IX register — always use SP-relative addressing.
  if (STI.hasSM83())
    return false;

  const MachineFrameInfo &MFI = MF.getFrameInfo();
  // Use IX as frame pointer when:
  // - Frame pointer elimination is disabled (-O0 or -fno-omit-frame-pointer)
  // - Variable-sized allocas require a stable base register
  // - Frame address is explicitly taken (e.g., varargs)
  // Otherwise, IX is freed for register allocation and stack access
  // uses SP-relative addressing (LD HL,offset; ADD HL,SP).
  return MF.getTarget().Options.DisableFramePointerElim(MF) ||
         MFI.hasVarSizedObjects() || MFI.isFrameAddressTaken();
}

void Z80FrameLowering::processFunctionBeforeFrameFinalized(
    MachineFunction &MF, RegScavenger *RS) const {
  // SP is at an arbitrary address when a function is entered and SM83's
  // SP-relative addressing rules out realigning it, so an alignment above
  // the byte-aligned stack cannot be honored. Anything that genuinely needs
  // one (an OAM DMA source buffer, say) silently receives an arbitrary
  // address today, so refuse loudly instead: the linker can place a static
  // object at any alignment.
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  Align Requested = getStackAlign();
  for (int I = MFI.getObjectIndexBegin(), E = MFI.getObjectIndexEnd(); I != E;
       ++I) {
    if (MFI.isDeadObjectIndex(I))
      continue;
    Requested = std::max(Requested, MFI.getObjectAlign(I));
  }
  if (Requested > getStackAlign()) {
    const Function &F = MF.getFunction();
    F.getContext().diagnose(DiagnosticInfoUnsupported(
        F,
        "an over-aligned stack object (alignment " + Twine(Requested.value()) +
            ", but the stack is byte-aligned); use a static object for "
            "aligned data",
        F.getSubprogram()));
  }
}

bool Z80FrameLowering::hasReservedCallFrame(const MachineFunction &MF) const {
  // Z80 uses PUSH to pass stack arguments, so the call frame is not
  // preallocated. Return false so ADJCALLSTACKDOWN/UP are preserved
  // for expansion in eliminateCallFramePseudoInstr.
  return false;
}

MachineBasicBlock::iterator Z80FrameLowering::eliminateCallFramePseudoInstr(
    MachineFunction &MF, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator MI) const {
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  DebugLoc DL = MI->getDebugLoc();
  unsigned Opc = MI->getOpcode();

  if (Opc == Z80::ADJCALLSTACKDOWN) {
    // PUSHes in the call sequence will adjust SP downward.
    // Nothing to emit here.
    return MBB.erase(MI);
  }

  assert(Opc == Z80::ADJCALLSTACKUP && "Expected ADJCALLSTACKUP");

  // Operand 0 = total stack bytes (used by PEI for SPAdj tracking).
  // Operand 1 = bytes already cleaned up by callee (callee-cleanup convention).
  // Actual caller SP adjustment = op0 - op1.
  int64_t Amount = MI->getOperand(0).getImm();
  int64_t CalleeAmount = MI->getOperand(1).getImm();
  Amount -= CalleeAmount;
  if (Amount == 0)
    return MBB.erase(MI);

  // Clean up stack after call: restore SP by adding Amount. Every branch
  // must agree with callFrameDestroyScratch below, which is how call
  // lowering and liveness queries predict what this expansion touches.
  const auto &STI = MF.getSubtarget<Z80Subtarget>();
  if (STI.hasSM83() && Amount <= 127) {
    // SM83: ADD SP,e (2 bytes, doesn't clobber HL)
    assert(callFrameDestroyScratch(STI, Amount) == Register());
    BuildMI(MBB, MI, DL, TII.get(Z80::ADD_SP_e)).addImm(Amount & 0xFF);
  } else if (Amount <= 16) {
    // Small amounts: use POP AF (each pops 2 bytes, clobbers A but not HL).
    // More compact than INC SP (1 byte per 2 bytes vs 1 byte per 1 byte).
    assert(callFrameDestroyScratch(STI, Amount) == Register(Z80::A));
    unsigned PopCount = Amount / 2;
    for (unsigned i = 0; i < PopCount; ++i)
      BuildMI(MBB, MI, DL, TII.get(Z80::POP_AF));
    if (Amount % 2)
      BuildMI(MBB, MI, DL, TII.get(Z80::INC_SP));
  } else {
    // Larger amounts: LD HL, Amount; ADD HL, SP; LD SP, HL
    assert(callFrameDestroyScratch(STI, Amount) == Register(Z80::HL));
    BuildMI(MBB, MI, DL, TII.get(Z80::LD_HL_nn)).addImm(Amount);
    BuildMI(MBB, MI, DL, TII.get(Z80::ADD_HL_SP));
    BuildMI(MBB, MI, DL, TII.get(Z80::LD_SP_HL));
  }

  return MBB.erase(MI);
}

Register Z80FrameLowering::callFrameDestroyScratch(const Z80Subtarget &STI,
                                                   int64_t CallerPopBytes) {
  if (CallerPopBytes == 0)
    return Register(); // erased entirely
  if (STI.hasSM83() && CallerPopBytes <= 127)
    return Register(); // ADD SP,e
  if (CallerPopBytes <= 16)
    return Z80::A; // POP AF per two bytes
  return Z80::HL;  // LD HL,N; ADD HL,SP; LD SP,HL
}

void Z80FrameLowering::emitPrologue(MachineFunction &MF,
                                    MachineBasicBlock &MBB) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineBasicBlock::iterator MBBI = MBB.begin();
  DebugLoc DL;

  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  uint64_t StackSize = MFI.getStackSize();

  if (hasFP(MF)) {
    // --- Frame pointer mode (IX = frame pointer) ---
    bool NeedsFP = MFI.getNumFixedObjects() > 0 || MFI.isFrameAddressTaken() ||
                   MFI.hasVarSizedObjects();

    if (!StackSize && !NeedsFP)
      return;

    // Z80 prologue:
    // 1. PUSH IX          - Save old frame pointer
    // 2. LD IX,0; ADD IX,SP - IX = SP (frame pointer)
    // 3. Adjust SP for locals (only if StackSize > 0)
    BuildMI(MBB, MBBI, DL, TII.get(Z80::PUSH_IX));
    BuildMI(MBB, MBBI, DL, TII.get(Z80::LD_IX_nn)).addImm(0);
    BuildMI(MBB, MBBI, DL, TII.get(Z80::ADD_IX_SP));

    if (StackSize > 0) {
      unsigned PushCount = StackSize / 2;
      if (PushCount <= 4) {
        for (unsigned i = 0; i < PushCount; ++i)
          emitStackAllocPush(MBB, MBBI, DL, TII);
        if (StackSize % 2)
          BuildMI(MBB, MBBI, DL, TII.get(Z80::DEC_SP));
      } else {
        // Large frame: PUSH HL; LD HL,-(size-2); ADD HL,SP; LD SP,HL;
        // restore HL from IX-based save location.
        MachineInstrBuilder PushHL = BuildMI(MBB, MBBI, DL, TII.get(Z80::PUSH_HL));
        // The save only matters when HL carries an incoming argument.
        if (!MBB.isLiveIn(Z80::HL) && !MBB.isLiveIn(Z80::L) &&
            !MBB.isLiveIn(Z80::H))
          Z80::markUndefUse(PushHL, Z80::HL);
        BuildMI(MBB, MBBI, DL, TII.get(Z80::LD_HL_nn))
            .addImm(-(int64_t)(StackSize - 2) & 0xFFFF);
        BuildMI(MBB, MBBI, DL, TII.get(Z80::ADD_HL_SP));
        BuildMI(MBB, MBBI, DL, TII.get(Z80::LD_SP_HL));
        BuildMI(MBB, MBBI, DL, TII.get(Z80::LD_L_IXd)).addImm(-2);
        BuildMI(MBB, MBBI, DL, TII.get(Z80::LD_H_IXd)).addImm(-1);
      }
    }
  } else {
    // --- No frame pointer (IX is allocatable) ---
    // Callee-saved registers are already pushed by spillCalleeSavedRegisters.
    // We only need to allocate space for locals (StackSize - CSSize).
    const Z80FunctionInfo *FI = MF.getInfo<Z80FunctionInfo>();
    uint64_t LocalSize = StackSize - FI->getCalleeSavedFrameSize();

    if (LocalSize == 0)
      return;

    // Skip past callee-saved PUSHes (inserted before prologue by PEI).
    while (MBBI != MBB.end() && MBBI->getFlag(MachineInstr::FrameSetup) &&
           MBBI->getOpcode() != Z80::ADJCALLSTACKDOWN) {
      ++MBBI;
    }

    const auto &STI = MF.getSubtarget<Z80Subtarget>();
    if (STI.hasSM83() && LocalSize <= 128) {
      // SM83: ADD SP,e (2 bytes, doesn't clobber HL)
      BuildMI(MBB, MBBI, DL, TII.get(Z80::ADD_SP_e))
          .addImm(-(int64_t)LocalSize & 0xFF);
    } else {
      unsigned PushCount = LocalSize / 2;
      // Check if HL might hold an incoming argument (live-in to entry block).
      // If so, we must not clobber HL with the large-frame LD HL approach.
      bool HLLive =
          MBB.isLiveIn(Z80::HL) || MBB.isLiveIn(Z80::H) || MBB.isLiveIn(Z80::L);
      if (HLLive || PushCount <= 12) {
        // Use PUSH AF (1 byte per 2 bytes, doesn't clobber any registers).
        for (unsigned i = 0; i < PushCount; ++i)
          emitStackAllocPush(MBB, MBBI, DL, TII);
        if (LocalSize % 2)
          BuildMI(MBB, MBBI, DL, TII.get(Z80::DEC_SP));
      } else {
        // Large frame, HL not live: LD HL, -LocalSize; ADD HL, SP; LD SP, HL
        BuildMI(MBB, MBBI, DL, TII.get(Z80::LD_HL_nn))
            .addImm(-(int64_t)LocalSize & 0xFFFF);
        BuildMI(MBB, MBBI, DL, TII.get(Z80::ADD_HL_SP));
        BuildMI(MBB, MBBI, DL, TII.get(Z80::LD_SP_HL));
      }
    }
  }
}

void Z80FrameLowering::emitEpilogue(MachineFunction &MF,
                                    MachineBasicBlock &MBB) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineBasicBlock::iterator MBBI = MBB.getLastNonDebugInstr();
  DebugLoc DL;
  if (MBBI != MBB.end())
    DL = MBBI->getDebugLoc();

  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  uint64_t StackSize = MFI.getStackSize();

  if (hasFP(MF)) {
    bool NeedsFP = MFI.getNumFixedObjects() > 0 || MFI.isFrameAddressTaken() ||
                   MFI.hasVarSizedObjects();

    if (!StackSize && !NeedsFP)
      return;

    // Z80 epilogue (before RET):
    // 1. LD SP,IX   - Restore stack pointer from frame pointer
    // 2. POP IX     - Restore old frame pointer
    BuildMI(MBB, MBBI, DL, TII.get(Z80::LD_SP_IX));
    BuildMI(MBB, MBBI, DL, TII.get(Z80::POP_IX));
  } else {
    // No frame pointer: deallocate locals only.
    // Callee-saved registers are restored by restoreCalleeSavedRegisters,
    // which inserts POPs (with FrameDestroy flag) before the return.
    // We must insert local deallocation BEFORE those callee-save restores,
    // since the stack layout is: [locals | callee-saves | ret-addr].
    const Z80FunctionInfo *FI = MF.getInfo<Z80FunctionInfo>();
    uint64_t LocalSize = StackSize - FI->getCalleeSavedFrameSize();

    if (LocalSize == 0)
      return;

    // Walk MBBI backwards past callee-save restore POPs (FrameDestroy flag)
    // so we insert local deallocation before them.
    while (MBBI != MBB.begin()) {
      auto Prev = std::prev(MBBI);
      if (Prev->getFlag(MachineInstr::FrameDestroy))
        MBBI = Prev;
      else
        break;
    }

    const auto &STI = MF.getSubtarget<Z80Subtarget>();
    if (STI.hasSM83() && LocalSize <= 127) {
      // SM83: ADD SP,e (2 bytes, doesn't clobber HL)
      BuildMI(MBB, MBBI, DL, TII.get(Z80::ADD_SP_e)).addImm(LocalSize & 0xFF);
    } else if (LocalSize <= 4) {
      // Small: INC SP loop (1 byte each, no clobber).
      for (unsigned i = 0; i < LocalSize; ++i)
        BuildMI(MBB, MBBI, DL, TII.get(Z80::INC_SP));
    } else {
      // Check which registers are live at return to pick the best strategy.
      // Return instruction adds implicit uses for return value registers.
      auto RetIt = MBB.getLastNonDebugInstr();
      bool HLLive = false, ALive = false;
      if (RetIt != MBB.end()) {
        for (const auto &MO : RetIt->operands()) {
          if (!MO.isReg() || !MO.isUse())
            continue;
          Register Reg = MO.getReg();
          if (Reg == Z80::HL || Reg == Z80::H || Reg == Z80::L)
            HLLive = true;
          if (Reg == Z80::A || Reg == Z80::AF)
            ALive = true;
        }
      }

      if (!HLLive) {
        // HL free: LD HL,LocalSize; ADD HL,SP; LD SP,HL (5 bytes total).
        BuildMI(MBB, MBBI, DL, TII.get(Z80::LD_HL_nn))
            .addImm(LocalSize & 0xFFFF);
        BuildMI(MBB, MBBI, DL, TII.get(Z80::ADD_HL_SP));
        BuildMI(MBB, MBBI, DL, TII.get(Z80::LD_SP_HL));
      } else if (!ALive) {
        // A free: POP AF loop (1 byte per 2 bytes).
        unsigned PopCount = LocalSize / 2;
        for (unsigned i = 0; i < PopCount; ++i)
          BuildMI(MBB, MBBI, DL, TII.get(Z80::POP_AF));
        if (LocalSize % 2)
          BuildMI(MBB, MBBI, DL, TII.get(Z80::INC_SP));
      } else {
        // Both A and HL live (i32 return): INC SP loop as last resort.
        for (unsigned i = 0; i < LocalSize; ++i)
          BuildMI(MBB, MBBI, DL, TII.get(Z80::INC_SP));
      }
    }
  }
}

void Z80FrameLowering::determineCalleeSaves(MachineFunction &MF,
                                            BitVector &SavedRegs,
                                            RegScavenger *RS) const {
  TargetFrameLowering::determineCalleeSaves(MF, SavedRegs, RS);
  // Note: when hasFP(MF), IX is reserved and manually saved/restored in
  // emitPrologue/emitEpilogue — do NOT add it to SavedRegs here, as that
  // would cause PEI to insert a redundant PUSH/POP via
  // spillCalleeSavedRegisters.
}

bool Z80FrameLowering::spillCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    ArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  if (CSI.empty())
    return true;

  MachineFunction &MF = *MBB.getParent();
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  DebugLoc DL;
  if (MI != MBB.end())
    DL = MI->getDebugLoc();

  // Track callee-saved frame size for prologue/epilogue local-only allocation.
  Z80FunctionInfo *FI = MF.getInfo<Z80FunctionInfo>();
  FI->setCalleeSavedFrameSize(CSI.size() * 2);

  // Spill registers using PUSH
  for (const CalleeSavedInfo &CS : CSI) {
    Register Reg = CS.getReg();
    unsigned Opcode = Z80::getPushOpcode(Reg);

    if (Opcode) {
      BuildMI(MBB, MI, DL, TII.get(Opcode)).setMIFlag(MachineInstr::FrameSetup);
    } else {
      llvm_unreachable("Unexpected CSR without push opcode");
    }
  }

  return true;
}

bool Z80FrameLowering::restoreCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    MutableArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  if (CSI.empty())
    return true;

  MachineFunction &MF = *MBB.getParent();
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  DebugLoc DL;
  if (MI != MBB.end())
    DL = MI->getDebugLoc();

  // Restore registers using POP in reverse order
  for (auto I = CSI.rbegin(), E = CSI.rend(); I != E; ++I) {
    Register Reg = I->getReg();
    unsigned Opcode = Z80::getPopOpcode(Reg);

    if (Opcode) {
      BuildMI(MBB, MI, DL, TII.get(Opcode))
          .setMIFlag(MachineInstr::FrameDestroy);
    } else {
      llvm_unreachable("Unexpected CSR without pop opcode");
    }
  }

  return true;
}
