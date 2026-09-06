//===-- Z80FrameLowering.h - Define frame lowering for Z80 ------*- C++ -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the Z80 declaration of TargetFrameLowering class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_Z80_Z80FRAMELOWERING_H
#define LLVM_LIB_TARGET_Z80_Z80FRAMELOWERING_H

#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/TargetFrameLowering.h"

namespace llvm {

class Z80Subtarget;

class Z80FrameLowering : public TargetFrameLowering {
public:
  Z80FrameLowering();

  bool spillCalleeSavedRegisters(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator MI,
                                 ArrayRef<CalleeSavedInfo> CSI,
                                 const TargetRegisterInfo *TRI) const override;

  bool
  restoreCalleeSavedRegisters(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator MI,
                              MutableArrayRef<CalleeSavedInfo> CSI,
                              const TargetRegisterInfo *TRI) const override;

  void determineCalleeSaves(MachineFunction &MF, BitVector &SavedRegs,
                            RegScavenger *RS) const override;

  MachineBasicBlock::iterator
  eliminateCallFramePseudoInstr(MachineFunction &MF, MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator MI) const override;

  /// The scratch register ADJCALLSTACKUP's expansion burns to pop
  /// \p CallerPopBytes, or 0 when the expansion touches none. The single
  /// authority for the policy eliminateCallFramePseudoInstr implements;
  /// call lowering declares the instance's clobbers from it and liveness
  /// queries consult it.
  static Register callFrameDestroyScratch(const Z80Subtarget &STI,
                                          int64_t CallerPopBytes);

  bool hasReservedCallFrame(const MachineFunction &MF) const override;

  // Refuses over-aligned stack objects: SP is arbitrary at entry, so they
  // cannot be honored, and silently placing them unaligned miscompiles
  // hardware-facing code like an OAM DMA source buffer.
  void processFunctionBeforeFrameFinalized(MachineFunction &MF,
                                           RegScavenger *RS) const override;
  // Ensure replaceFrameIndices runs even when there are no stack objects,
  // so that ADJCALLSTACKDOWN/UP pseudos are always eliminated.
  bool needsFrameIndexResolution(const MachineFunction &MF) const override {
    return MF.getFrameInfo().hasStackObjects() ||
           MF.getFrameInfo().adjustsStack();
  }

  void emitPrologue(MachineFunction &MF, MachineBasicBlock &MBB) const override;
  void emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB) const override;

  /// True when this function's locals live in static memory instead of on
  /// the stack.
  bool usesStaticFrame(const MachineFunction &MF) const;

  /// Total size of the frame objects moved to static memory.
  uint64_t staticFrameSize(const MachineFrameInfo &MFI) const;

private:
  bool hasFPImpl(const MachineFunction &MF) const override;
};

} // namespace llvm

#endif // not LLVM_LIB_TARGET_Z80_Z80FRAMELOWERING_H
