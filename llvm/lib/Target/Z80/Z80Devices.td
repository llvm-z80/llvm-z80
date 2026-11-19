//===----------------------------------------------------------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Z80 Device/Processor Definitions
//
//===----------------------------------------------------------------------===//

class Proc<string Name, list<SubtargetFeature> Features,
           SchedMachineModel Model = NoSchedModel>
    : ProcessorModel<Name, Model, !listconcat(Features, [FeatureStaticFrame])>;

// Zilog Z80
def : Proc<"z80", [FeatureZ80], Z80Model>;

// Zilog Z80 with undocumented instructions enabled
def : Proc<"z80-undoc", [FeatureZ80, FeatureUndocumented], Z80Model>;

// Sharp SM83 (GBZ80)
def : Proc<"sm83", [FeatureSM83], SM83Model>;
def : ProcessorAlias<"gbz80", "sm83">;
