//===-- Z80Devices.td - Z80 Devices -----------------------*- tablegen -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//
// Z80 Device/Processor Definitions
//===---------------------------------------------------------------------===//

class Proc<string Name, list<SubtargetFeature> Features>
    : Processor<Name, NoItineraries, Features>;

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
// Retro Computing Targets
//===---------------------------------------------------------------------===//

// ZX Spectrum
def : Proc<"zxspectrum", [FeatureZ80]>;

// MSX
def : Proc<"msx", [FeatureZ80]>;

// MSX2
def : Proc<"msx2", [FeatureZ80]>;

// MSX TurboR
def : Proc<"msxtr", [FeatureR800]>;

// Amstrad CPC
def : Proc<"cpc", [FeatureZ80]>;

// TRS-80
def : Proc<"trs80", [FeatureZ80]>;

// CP/M systems
def : Proc<"cpm", [FeatureZ80]>;

// Sega Master System
def : Proc<"sms", [FeatureZ80]>;

// Sega Game Gear
def : Proc<"gamegear", [FeatureZ80]>;

// ColecoVision
def : Proc<"coleco", [FeatureZ80]>;

// TI-83/84 calculators (eZ80)
def : Proc<"ti84pce", [FeatureEZ80]>;

// Generic target (alias for z80)
def : Proc<"generic", [FeatureZ80]>;

//===---------------------------------------------------------------------===//
// Sharp SM83 (Game Boy)
//===---------------------------------------------------------------------===//

// Sharp SM83 (LR35902)
def : Proc<"sm83", [FeatureSM83]>;

// Nintendo Game Boy
def : Proc<"gameboy", [FeatureSM83]>;

// Nintendo Game Boy Color
def : Proc<"gbc", [FeatureSM83]>;
