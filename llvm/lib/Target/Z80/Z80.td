//===-- Z80.td - Describe the Z80 Target Machine -------------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//
// This is the top level entry point for the Z80 target.
//===---------------------------------------------------------------------===//

//===---------------------------------------------------------------------===//
// Target-independent interfaces
//===---------------------------------------------------------------------===//

include "llvm/Target/Target.td"

//===---------------------------------------------------------------------===//
// Z80 Device Definitions
//===---------------------------------------------------------------------===//

include "Z80Features.td"
include "Z80RegisterInfo.td"
include "Z80RegisterBanks.td"
include "Z80InstrInfo.td"
include "Z80Schedule.td"
include "Z80Devices.td"
include "Z80Combine.td"

include "Z80CallingConv.td"
include "SM83CallingConv.td"

def Z80InstrInfoDef : InstrInfo {
  let guessInstructionProperties = false;
}

def Z80AsmWriter : AsmWriter {
  string AsmWriterClassName = "InstPrinter";
}

def Z80AsmParser : AsmParser {
  let ShouldEmitMatchRegisterAltName = true;
}

//===---------------------------------------------------------------------===//
// Target Declaration
//===---------------------------------------------------------------------===//

defm : RemapAllTargetPseudoPointerOperands<GR16>;

def Z80 : Target {
  let InstructionSet         = Z80InstrInfoDef;
  let AllowRegisterRenaming  = true;
  let AssemblyWriters        = [Z80AsmWriter];
  let AssemblyParsers        = [Z80AsmParser];
}
