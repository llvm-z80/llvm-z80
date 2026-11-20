//===--- BuiltinsZ80.td - Z80 Builtin function database ---------*- C++ -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Built-ins for Z80 privileged instructions that cannot be expressed in C.
// Each lowers to the matching llvm.z80.* intrinsic.  None is const/pure, so
// they carry side effects and are never CSE'd or removed.  ravn/llvm-z80#42.
//
//===----------------------------------------------------------------------===//

include "clang/Basic/BuiltinsBase.td"

// Disable interrupts (DI).
def Z80DI : TargetBuiltin {
  let Spellings = ["__builtin_z80_di"];
  let Attributes = [NoThrow];
  let Prototype = "void()";
}

// Enable interrupts (EI).
def Z80EI : TargetBuiltin {
  let Spellings = ["__builtin_z80_ei"];
  let Attributes = [NoThrow];
  let Prototype = "void()";
}

// Halt the CPU until the next interrupt (HALT).
def Z80Halt : TargetBuiltin {
  let Spellings = ["__builtin_z80_halt"];
  let Attributes = [NoThrow];
  let Prototype = "void()";
}

// No operation (NOP) -- useful for cycle-accurate timing.
def Z80Nop : TargetBuiltin {
  let Spellings = ["__builtin_z80_nop"];
  let Attributes = [NoThrow];
  let Prototype = "void()";
}

// Select interrupt mode 2 (IM 2).  Z80-only: SM83 (Game Boy) has no interrupt
// modes, so this is gated on the "z80" feature (see Z80TargetInfo) to produce a
// clean Sema diagnostic on sm83 instead of a backend cannot-select.
def Z80Im2 : TargetBuiltin {
  let Spellings = ["__builtin_z80_im2"];
  let Attributes = [NoThrow];
  let Prototype = "void()";
  let Features = "z80";
}

// Load the interrupt vector register I from the given value (LD I,A).  Z80-only:
// SM83 has no I register; gated on the "z80" feature (see Z80TargetInfo).
def Z80SetI : TargetBuiltin {
  let Spellings = ["__builtin_z80_set_i"];
  let Attributes = [NoThrow];
  let Prototype = "void(unsigned char)";
  let Features = "z80";
}
