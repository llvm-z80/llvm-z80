//===-- Z80.td - Describe the Z80 Target Machine ----------*- tablegen -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//
// This is the top level entry point for the Z80 target.
//===---------------------------------------------------------------------===//

//===---------------------------------------------------------------------===//
// Target-independent interfaces which we are implementing
//===---------------------------------------------------------------------===//

include "llvm/Target/Target.td"
include "llvm/Target/GlobalISel/Combine.td"
include "llvm/TableGen/SearchableTable.td"

//===---------------------------------------------------------------------===//
// Z80 Device Definitions
//===---------------------------------------------------------------------===//

include "Z80Features.td"
include "Z80Devices.td"
include "Z80RegisterInfo.td"
include "Z80RegisterBanks.td"
include "Z80InstrInfo.td"
include "Z80Combine.td"

def Z80InstrInfoDef : InstrInfo {
  let guessInstructionProperties = 0;
}

include "Z80CallingConv.td"
include "SM83CallingConv.td"

def Z80AsmWriter : AsmWriter {
 string AsmWriterClassName = "InstPrinter";
 bit isMCAsmWriter = 1;
}

def Z80AsmParser : AsmParser {
  let ShouldEmitMatchRegisterAltName = 1;
  let ReportMultipleNearMisses = 0;
  string RegisterPrefix = "";
  string TokenizingCharacters = "#(),[]*!";
}

def Z80AsmParserVariantDefault : AsmParserVariant {
  int Variant = 0;
  let Name = "generic";
}

def Z80AsmParserVariantZilog : AsmParserVariant {
  int Variant = 1;
  let Name = "zilog";
}

//===---------------------------------------------------------------------===//
// Target Declaration
//===---------------------------------------------------------------------===//

defm : RemapAllTargetPseudoPointerOperands<Ptr16>;

def Z80 : Target {
  let InstructionSet         = Z80InstrInfoDef;
  let AssemblyWriters        = [Z80AsmWriter];
  let AllowRegisterRenaming = 1;

  let AssemblyParsers        = [Z80AsmParser];
  let AssemblyParserVariants = [Z80AsmParserVariantDefault,
                                Z80AsmParserVariantZilog];
}
