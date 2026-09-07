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

  // First-argument register for an i8 argument.  Only __z88dk_fastcall reads
  // it; sdcccall(1) hardcodes the first i8 argument to A, and the stack
  // bases pass nothing in registers.
  Register First_I8; // z88dk fastcall: L (Z80) / E (SM83)

  // The pair __sdcccall(1) does not reach, which CallingConv::Z80_Builtin adds
  // as a third argument register.  Z80 passes in HL, DE and then this; SM83
  // passes in DE, BC and then this.
  Register Third_I16; // Z80: BC, SM83: HL

  // Low halves of the three pairs above, in the same order, for a narrow
  // argument that lands past the accumulator.
  Register Half_1, Half_2, Half_3; // Z80: L, E, C   SM83: E, C, L
};

/// The argument-passing scheme a Z80 calling convention is built on.  SDCC
/// spells these as mutually exclusive keywords: __smallc overrides whichever
/// __sdcccall level is in effect, and __z88dk_fastcall overrides everything.
enum class Z80CCBase {
  SDCCCall1,     ///< The default: first two arguments in registers, rest on
                 ///< the stack right-to-left.  Returns in A/DE/HL:DE.
  SDCCCall0,     ///< Every argument on the stack, right-to-left.
  SmallC,        ///< Every argument on the stack, left-to-right.
  Z88dkFastCall, ///< A single argument, in the return registers.
  Builtin,       ///< Backend-internal rtlib helpers: everything in registers.
};

/// What a Z80 calling convention actually does.  SDCC builds its conventions
/// from an argument-passing base plus the __z88dk_callee modifier, so deciding
/// behaviour from those rather than from a list of convention names keeps each
/// decision written once no matter how the combinations grow.
struct Z80CCAxes {
  Z80CCBase Base = Z80CCBase::SDCCCall1;

  /// __z88dk_callee: the callee pops the stack arguments on return, for every
  /// non-variadic call and regardless of the return size.  __sdcccall(1)
  /// otherwise hands that job back to the caller once the return exceeds
  /// 16 bits.
  bool ForcedCalleeCleanup = false;

  /// No argument is passed in a register.
  bool stackArgsOnly() const {
    return Base == Z80CCBase::SDCCCall0 || Base == Z80CCBase::SmallC;
  }
  /// __smallc order: the first declared argument is pushed first, so the
  /// callee finds the LAST one nearest the return address.
  bool isLeftToRight() const { return Base == Z80CCBase::SmallC; }
  bool isFastCall() const { return Base == Z80CCBase::Z88dkFastCall; }
  /// A backend-internal rtlib helper, whose arguments are all in registers.
  bool isBuiltin() const { return Base == Z80CCBase::Builtin; }

  /// True for the bases that return in the z88dk classic registers
  /// (Z80 i8->L, i16->HL, i32->DE:HL) rather than in __sdcccall(1)'s.
  bool usesZ88dkRegs() const {
    return Base != Z80CCBase::SDCCCall1 && Base != Z80CCBase::Builtin;
  }
};

/// Decode \p CC into its base and modifiers.  This is the only place that
/// knows which convention is built from what; everything else asks the axes.
/// Any convention outside the set, meaning CallingConv::C (SDCC
/// __sdcccall(1)), decodes to the plain default.
inline Z80CCAxes decodeZ80CC(CallingConv::ID CC) {
  Z80CCAxes Axes;
  switch (CC) {
  case CallingConv::Z80_SDCCCall0:
    Axes.Base = Z80CCBase::SDCCCall0;
    break;
  case CallingConv::Z80_SmallC:
    Axes.Base = Z80CCBase::SmallC;
    break;
  case CallingConv::Z80_Z88dkFastCall:
    Axes.Base = Z80CCBase::Z88dkFastCall;
    break;
  case CallingConv::Z80_Builtin:
    Axes.Base = Z80CCBase::Builtin;
    break;
  case CallingConv::Z80_Z88dkCallee:
    Axes.ForcedCalleeCleanup = true;
    break;
  case CallingConv::Z80_SDCCCall0Callee:
    Axes.Base = Z80CCBase::SDCCCall0;
    Axes.ForcedCalleeCleanup = true;
    break;
  case CallingConv::Z80_SmallCCallee:
    Axes.Base = Z80CCBase::SmallC;
    Axes.ForcedCalleeCleanup = true;
    break;
  default:
    break;
  }
  return Axes;
}

/// Common call lowering implementation for Z80-family targets.
/// All logic is parameterized by CallingConvRegs.
class Z80CallLoweringCommon : public CallLowering {
protected:
  CallingConvRegs CCRegs;  // sdcccall(1) registers (the default convention)
  CallingConvRegs CCRegs0; // z88dk/SDCC block registers (__sdcccall(0) et al.)

  /// Select the register config for \p CC.  Every base other than
  /// __sdcccall(1) shares one set: the stack ones only ever read its return
  /// registers, and __z88dk_fastcall additionally passes its sole argument in
  /// them.
  const CallingConvRegs &getRegsForCC(CallingConv::ID CC) const {
    return decodeZ80CC(CC).usesZ88dkRegs() ? CCRegs0 : CCRegs;
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
