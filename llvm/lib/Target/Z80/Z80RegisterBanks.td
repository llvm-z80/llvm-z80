//===- Z80RegisterBanks.td - Z80 Register Bank Description -*- tablegen -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes the GlobalISel Register Banks for Z80.
//
//===----------------------------------------------------------------------===//

// Z80 has limited registers, so place everything in one bank.
// Use the register classes defined in Z80RegisterInfo.td:
// - GR8 (8-bit GPRs)
// - GR16 (16-bit register pairs)
// - Flags (flag pseudo-registers)
// - Fakei32, Fakei64 (for multi-word values)
def AnyRegBank : RegisterBank<"Any", [GR8, GR16, Fakei32, Fakei64]>;
