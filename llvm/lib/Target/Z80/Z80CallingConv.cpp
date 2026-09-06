//===-- Z80CallingConv.cpp - Z80 Calling Convention
//------------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the Z80 calling convention.
//
//===----------------------------------------------------------------------===//

#include "Z80CallingConv.h"

#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/IR/DataLayout.h"

#include "Z80GenCallingConv.inc"
