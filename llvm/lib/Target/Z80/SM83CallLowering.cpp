//===-- SM83CallLowering.cpp - SM83 Call lowering ----------------*- C++
//-*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// SM83 (Game Boy CPU) call lowering. All logic is in Z80CallLoweringCommon;
// this file only provides the SM83-specific register configuration.
//
//===----------------------------------------------------------------------===//

#include "SM83CallLowering.h"
#include "MCTargetDesc/Z80MCTargetDesc.h"

using namespace llvm;

SM83CallLowering::SM83CallLowering(const TargetLowering *TL)
    : Z80CallLoweringCommon(TL,
                            // sdcccall(1) registers
                            CallingConvRegs{
                                /*First_I16=*/Z80::DE,
                                /*First_I32_Hi=*/Z80::DE,
                                /*First_I32_Lo=*/Z80::BC,
                                /*Second_AfterI8_I8=*/Z80::E,
                                /*Second_AfterI8_I16=*/Z80::DE,
                                /*Second_AfterI16_I8=*/Z80::A,
                                /*Second_AfterI16_I16=*/Z80::BC,
                                /*Ret_I8=*/Z80::A,
                                /*Ret_I16=*/Z80::BC,
                                /*Ret_I32_Hi=*/Z80::DE,
                                /*Ret_I32_Lo=*/Z80::BC,
                                /*IndirectCallReg=*/Z80::HL,
                                /*IndirectCallOpc=*/Z80::CALL_HL,
                            },
                            // sdcccall(0) registers
                            CallingConvRegs{
                                /*First_I16=*/Register(),
                                /*First_I32_Hi=*/Register(),
                                /*First_I32_Lo=*/Register(),
                                /*Second_AfterI8_I8=*/Register(),
                                /*Second_AfterI8_I16=*/Register(),
                                /*Second_AfterI16_I8=*/Register(),
                                /*Second_AfterI16_I16=*/Register(),
                                /*Ret_I8=*/Z80::E,
                                /*Ret_I16=*/Z80::DE,
                                /*Ret_I32_Hi=*/Z80::HL,
                                /*Ret_I32_Lo=*/Z80::DE,
                                /*IndirectCallReg=*/Z80::HL,
                                /*IndirectCallOpc=*/Z80::CALL_HL,
                            }) {}
