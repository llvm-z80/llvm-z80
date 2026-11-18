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
    : SubtargetFeature<"z80", "Z80ProcFamily", "Z80ProcFamilyEnum::Z80",
                       "Zilog Z80 instruction set">;

// Sharp SM83 instructions (or GBZ80, Game Boy CPU)
def FeatureSM83
    : SubtargetFeature<"sm83", "Z80ProcFamily", "Z80ProcFamilyEnum::SM83",
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
