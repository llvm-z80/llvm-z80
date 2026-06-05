//===-- SM83CallLowering.h - Call lowering -----------------------*- C++
//-*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// SM83 (Game Boy CPU) call lowering with SM83-specific register assignments.
// SM83 SDCC __sdcccall(1) differs from Z80:
//   - Args: 1st i8->A, 1st i16->DE, 2nd depends on 1st
//   - Returns: i8->A, i16->BC, i32->DEBC
//   - No callee-saved registers
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_Z80_SM83CALLLOWERING_H
#define LLVM_LIB_TARGET_Z80_SM83CALLLOWERING_H

#include "Z80CallLowering.h"

namespace llvm {

/// SM83 call lowering with SM83-specific register assignments.
class SM83CallLowering : public Z80CallLoweringCommon {
public:
  SM83CallLowering(const TargetLowering *TL);
};

} // namespace llvm

#endif // not LLVM_LIB_TARGET_Z80_SM83CALLLOWERING_H
