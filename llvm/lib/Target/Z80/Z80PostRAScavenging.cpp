//===-- Z80PostRAScavenging.cpp - Z80 Post RA Scavenging ------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the Z80 post-register-allocation register scavenging pass.
//
// This pass runs immediately after post-RA pseudo expansion. These pseudos
// (including COPY) often require temporary registers on Z80; moreso than on
// other platforms. Accordingly, they emit virtual registers instead, and this
// pass performs register scavenging to assign them to physical registers,
// freeing them up via save and restore if neccesary. A very similar process is
// performed in prologue/epilogue insertion.
//
//===----------------------------------------------------------------------===//

#include "Z80PostRAScavenging.h"

#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/RegisterScavenging.h"

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80.h"

#define DEBUG_TYPE "z80-scavenging"

using namespace llvm;

namespace {

class Z80PostRAScavenging : public MachineFunctionPass {
public:
  static char ID;

  Z80PostRAScavenging() : MachineFunctionPass(ID) {
    llvm::initializeZ80PostRAScavengingPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
};

bool Z80PostRAScavenging::runOnMachineFunction(MachineFunction &MF) {
  if (MF.getProperties().hasProperty(
          MachineFunctionProperties::Property::NoVRegs))
    return false;

  RegScavenger RS;
  scavengeFrameVirtualRegs(MF, RS);

  return true;
}

} // namespace

char Z80PostRAScavenging::ID = 0;

INITIALIZE_PASS(Z80PostRAScavenging, DEBUG_TYPE,
                "Scavenge virtual registers emitted by post-RA pseudos", false,
                false)

MachineFunctionPass *llvm::createZ80PostRAScavengingPass() {
  return new Z80PostRAScavenging();
}
