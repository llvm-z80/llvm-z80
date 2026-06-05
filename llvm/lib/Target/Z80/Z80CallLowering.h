//===-- Z80CallLowering.h - Call lowering -----------------------*- C++ -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes how to lower LLVM calls to machine code calls.
// Z80CallLoweringCommon provides the shared implementation parameterized
// by CallingConvRegs; Z80CallLowering and SM83CallLowering supply
// target-specific register mappings.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_Z80_Z80CALLLOWERING_H
#define LLVM_LIB_TARGET_Z80_Z80CALLLOWERING_H

#include "llvm/CodeGen/FunctionLoweringInfo.h"
#include "llvm/CodeGen/GlobalISel/CallLowering.h"
#include "llvm/CodeGen/Register.h"

namespace llvm {

/// Register assignments for SDCC __sdcccall(1) calling convention.
/// Z80 and SM83 share the same structure but use different physical registers.
struct CallingConvRegs {
  // classifyArg: 1st param
  Register First_I16;    // Z80: HL, SM83: DE
  Register First_I32_Hi; // Z80: HL, SM83: DE
  Register First_I32_Lo; // Z80: DE, SM83: BC

  // classifyArg: 2nd param after 1st=I8
  Register Second_AfterI8_I8;  // Z80: L,  SM83: E
  Register Second_AfterI8_I16; // Z80: DE, SM83: DE

  // classifyArg: 2nd param after 1st=I16
  Register Second_AfterI16_I8;  // Z80: Register() (none), SM83: A
  Register Second_AfterI16_I16; // Z80: DE, SM83: BC

  // Return value registers
  Register Ret_I8;     // Z80 cc1: A, cc0: L; SM83 cc1: A, cc0: E
  Register Ret_I16;    // Z80: DE, SM83: BC
  Register Ret_I32_Hi; // Z80: HL, SM83: DE
  Register Ret_I32_Lo; // Z80: DE, SM83: BC

  // Sret pointer is passed on the stack (SDCC convention), not in a register.
  // See Z80CallingConv.td / SM83CallingConv.td for details.

  // Indirect call
  Register IndirectCallReg; // Z80: IY, SM83: HL
  unsigned IndirectCallOpc; // Z80: CALL_IY, SM83: CALL_HL
};

/// Common call lowering implementation for Z80-family targets.
/// All logic is parameterized by CallingConvRegs.
class Z80CallLoweringCommon : public CallLowering {
protected:
  CallingConvRegs CCRegs;  // sdcccall(1) registers (default)
  CallingConvRegs CCRegs0; // sdcccall(0) registers

  /// Select the appropriate register config based on calling convention.
  const CallingConvRegs &getRegsForCC(CallingConv::ID CC) const {
    return CC == CallingConv::Z80_SDCCCall0 ? CCRegs0 : CCRegs;
  }

public:
  Z80CallLoweringCommon(const TargetLowering *TL, CallingConvRegs Regs,
                        CallingConvRegs Regs0)
      : CallLowering(TL), CCRegs(Regs), CCRegs0(Regs0) {}

  bool lowerReturn(MachineIRBuilder &MIRBuilder, const Value *Val,
                   ArrayRef<Register> VRegs,
                   FunctionLoweringInfo &FLI) const override;

  bool lowerFormalArguments(MachineIRBuilder &MIRBuilder, const Function &F,
                            ArrayRef<ArrayRef<Register>> VRegs,
                            FunctionLoweringInfo &FLI) const override;

  bool lowerCall(MachineIRBuilder &MIRBuilder,
                 CallLoweringInfo &Info) const override;

  bool canLowerReturn(MachineFunction &MF, CallingConv::ID CallConv,
                      SmallVectorImpl<BaseArgInfo> &Outs,
                      bool IsVarArg) const override;
};

/// Z80 call lowering with Z80-specific register assignments.
class Z80CallLowering : public Z80CallLoweringCommon {
public:
  Z80CallLowering(const TargetLowering *TL);
};

} // namespace llvm

#endif // not LLVM_LIB_TARGET_Z80_Z80CALLLOWERING_H
