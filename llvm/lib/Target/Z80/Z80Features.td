//===-- Z80Features.td - Z80 Features ------------------------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//
// Z80 Features and Subtarget Definitions
//===---------------------------------------------------------------------===//

// Default Z80 instructions
def FeatureZ80
    : SubtargetFeature<"z80", "HasZ80", "true",
                       "Zilog Z80 instruction set">;

// Z180 extensions
def FeatureZ180
    : SubtargetFeature<"z180", "HasZ180", "true",
                       "Zilog Z180 instruction set",
                       [FeatureZ80]>;

// R800 extensions (MSX TurboR)
def FeatureR800
    : SubtargetFeature<"r800", "HasR800", "true",
                       "ASCII R800 instruction set",
                       [FeatureZ80]>;

// eZ80 extensions (Z80-compatible mode)
def FeatureEZ80
    : SubtargetFeature<"ez80", "HasEZ80", "true",
                       "Zilog eZ80 (Z80 mode) instruction set",
                       [FeatureZ80]>;

// Sharp SM83 instructions (or GBZ80, Game Boy CPU)
def FeatureSM83
    : SubtargetFeature<"sm83", "HasSM83", "true",
                       "Sharp SM83 instruction set">;

// Undocumented Z80 instructions: 
// IXH/IXL/IYH/IYL half-index registers, SLL, IN/OUT with the C register, etc.
def FeatureUndocumented
    : SubtargetFeature<"undocumented", "HasUndocumented", "true",
                       "Enable undocumented Z80 instructions">;

// Static frame feature
// This compiler feature is called by various names in many compilers,
// such as "static stack", "compiled stack" or "overlay".
// The Z80 backend uses the term "Static Frame" to describe this feature.
def FeatureStaticFrame
    : SubtargetFeature<"static-frame", "StaticFrame", "true",
                       "Allow function frames to reside in a static section">;

// Inline-expand 16-bit runtime library calls
def FeatureInlineI16Runtime
    : SubtargetFeature<"inline-i16-runtime", "InlineI16Runtime", "true",
                       "Inline-expand 16-bit runtime library calls">;
