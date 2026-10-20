//===-- Z80Devices.td - Z80 Devices -----------------------*- tablegen -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//
// Z80 Device/Processor Definitions
//===---------------------------------------------------------------------===//

// Every processor allows static frames; a function opts out through the
// no-static-frame spelling of the target attribute.
class Proc<string Name, list<SubtargetFeature> Features>
    : Processor<Name, NoItineraries,
                !listconcat(Features, [FeatureStaticFrame])>;

//===---------------------------------------------------------------------===//
// Standard Z80 Processors
//===---------------------------------------------------------------------===//

// Original Zilog Z80
def : Proc<"z80", [FeatureZ80]>;

// Z80 with undocumented instructions enabled
def : Proc<"z80-undoc", [FeatureZ80, FeatureUndocumented]>;

//===---------------------------------------------------------------------===//
// Z180 Series
//===---------------------------------------------------------------------===//

// Zilog Z180 (enhanced Z80)
def : Proc<"z180", [FeatureZ180]>;

// Hitachi HD64180
def : Proc<"hd64180", [FeatureZ180]>;

//===---------------------------------------------------------------------===//
// R800 (MSX TurboR)
//===---------------------------------------------------------------------===//

def : Proc<"r800", [FeatureR800]>;

//===---------------------------------------------------------------------===//
// eZ80 Series (24-bit)
//===---------------------------------------------------------------------===//

def : Proc<"ez80", [FeatureEZ80]>;

//===---------------------------------------------------------------------===//
// Sharp SM83 (Game Boy)
//===---------------------------------------------------------------------===//

// Sharp SM83 (LR35902)
def : Proc<"sm83", [FeatureSM83]>;
def : ProcessorAlias<"gbz80", "sm83">;
