//===-- Z80TargetMachine.h - Define TargetMachine for Z80 -------*- C++ -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the Z80 specific subclass of TargetMachine.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_Z80_TARGET_MACHINE_H
#define LLVM_Z80_TARGET_MACHINE_H

#include "llvm/CodeGen/CodeGenTargetMachineImpl.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Target/TargetMachine.h"

#include "Z80FrameLowering.h"
#include "Z80ISelLowering.h"
#include "Z80InstrInfo.h"
#include "Z80Subtarget.h"

namespace llvm {

/// A generic Z80 implementation.
class Z80TargetMachine : public CodeGenTargetMachineImpl {
public:
  Z80TargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                   StringRef FS, const TargetOptions &Options,
                   std::optional<Reloc::Model> RM,
                   std::optional<CodeModel::Model> CM, CodeGenOptLevel OL,
                   bool JIT);

  const Z80Subtarget *getSubtargetImpl() const { return &SubTarget; }
  const Z80Subtarget *getSubtargetImpl(const Function &F) const override;

  TargetLoweringObjectFile *getObjFileLowering() const override {
    return this->TLOF.get();
  }

  TargetTransformInfo getTargetTransformInfo(const Function &F) const override;

  void registerPassBuilderCallbacks(PassBuilder &) override;

  TargetPassConfig *createPassConfig(PassManagerBase &PM) override;

  // The Z80 has only register-related scheduling concerns, so disable PostRA
  // scheduling by claiming to emit it ourselves, then never doing so.
  bool targetSchedulesPostRAScheduling() const override { return true; };

  MachineFunctionInfo *
  createMachineFunctionInfo(BumpPtrAllocator &Allocator, const Function &F,
                            const TargetSubtargetInfo *STI) const override;

private:
  std::unique_ptr<TargetLoweringObjectFile> TLOF;
  Z80Subtarget SubTarget;
  mutable StringMap<std::unique_ptr<Z80Subtarget>> SubtargetMap;
};

} // end namespace llvm

#endif // LLVM_Z80_TARGET_MACHINE_H
