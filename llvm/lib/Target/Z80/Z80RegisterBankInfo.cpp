//===- Z80RegisterBankInfo.cpp --------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the targeting of the RegisterBankInfo class for Z80.
//
// The Z80 doesn't really have register banks. A distinction could be made
// between the real and imaginary registers, but the Register Bank Selector
// doesn't take register pressure into account when allocating banks. Since the
// hardware registers are extremely tight, we have the bank selector allocate
// everything to the same "Any" register bank. The register allocator proper
// will later select real registers for each value, taking register pressure
// fully into account.
//
//===----------------------------------------------------------------------===//

#include "Z80RegisterBankInfo.h"
#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/RegisterBank.h"

#define GET_TARGET_REGBANK_IMPL
#include "Z80GenRegisterBank.inc"

using namespace llvm;

const RegisterBankInfo::InstructionMapping &
Z80RegisterBankInfo::getInstrMapping(const MachineInstr &MI) const {
  const auto &Mapping = getInstrMappingImpl(MI);
  if (Mapping.isValid())
    return Mapping;

  const auto &MRI = MI.getMF()->getRegInfo();
  unsigned NumOperands = MI.getNumOperands();

  SmallVector<const ValueMapping *, 8> ValMappings(NumOperands);
  for (const auto &I : enumerate(MI.operands())) {
    if (!I.value().isReg())
      continue;
    // Only the destination is expected for PHIs.
    if (MI.isPHI() && I.index() == 1) {
      NumOperands = 1;
      break;
    }
    LLT Ty = MRI.getType(I.value().getReg());
    if (!Ty.isValid())
      continue;
    ValMappings[I.index()] =
        &getValueMapping(0, Ty.getSizeInBits(), Z80::AnyRegBank);
  }
  return getInstructionMapping(/*ID=*/1, /*Cost=*/1,
                               getOperandsMapping(ValMappings), NumOperands);
}

void Z80RegisterBankInfo::applyMappingImpl(
    MachineIRBuilder &Builder, const OperandsMapper &OpdMapper) const {
  applyDefaultMapping(OpdMapper);
}

const RegisterBank &
Z80RegisterBankInfo::getRegBankFromRegClass(const TargetRegisterClass &RC,
                                            LLT Ty) const {
  return Z80::AnyRegBank;
}
