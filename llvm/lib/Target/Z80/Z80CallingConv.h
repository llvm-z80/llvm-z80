//===-- Z80CallingConv.h - Z80 Calling Convention-----------------*- C++
//-*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the Z80 calling convention.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_Z80_Z80CALLINGCONV_H
#define LLVM_LIB_TARGET_Z80_Z80CALLINGCONV_H

// The calling convention is implemented manually in Z80CallLowering.cpp
// following SDCC __sdcccall(1). CalleeSavedRegs are defined in
// Z80CallingConv.td and generated into Z80GenRegisterInfo.inc.

#endif // not LLVM_LIB_TARGET_Z80_Z80CALLINGCONV_H
