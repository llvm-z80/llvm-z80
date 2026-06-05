//===-- Z80TargetObjectFile.h - Z80 Object Info -----------------*- C++ -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_Z80_TARGET_OBJECT_FILE_H
#define LLVM_Z80_TARGET_OBJECT_FILE_H

#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"

namespace llvm {

/// Lowering for an Z80 ELF32 object file.
class Z80TargetObjectFile : public TargetLoweringObjectFileELF {
public:
  void Initialize(MCContext &ctx, const TargetMachine &TM) override;
  MCSection *getExplicitSectionGlobal(const GlobalObject *GO, SectionKind Kind,
                                      const TargetMachine &TM) const override;
};

} // end namespace llvm

#endif // LLVM_Z80_TARGET_OBJECT_FILE_H
