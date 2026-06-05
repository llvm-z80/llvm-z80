//===- IntrinsicsZ80.td - Defines Z80 intrinsics -----------*- tablegen -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines all of the Z80-specific intrinsics.
//
//===----------------------------------------------------------------------===//

let TargetPrefix = "z80" in {  // All intrinsics start with "llvm.z80."

  // Port I/O intrinsics
  // These map directly to Z80 IN and OUT instructions

  // Read a byte from an I/O port
  // i8 @llvm.z80.in(i8 port)
  def int_z80_in : Intrinsic<[llvm_i8_ty], [llvm_i8_ty],
                             [IntrNoMem, IntrHasSideEffects]>;

  // Write a byte to an I/O port
  // void @llvm.z80.out(i8 port, i8 value)
  def int_z80_out : Intrinsic<[], [llvm_i8_ty, llvm_i8_ty],
                              [IntrNoMem, IntrHasSideEffects]>;

  // Halt the CPU (useful for ending programs)
  // void @llvm.z80.halt()
  def int_z80_halt : Intrinsic<[], [], [IntrNoMem, IntrHasSideEffects]>;

  // Disable interrupts
  // void @llvm.z80.di()
  def int_z80_di : Intrinsic<[], [], [IntrNoMem, IntrHasSideEffects]>;

  // Enable interrupts
  // void @llvm.z80.ei()
  def int_z80_ei : Intrinsic<[], [], [IntrNoMem, IntrHasSideEffects]>;

  // No operation (useful for timing loops)
  // void @llvm.z80.nop()
  def int_z80_nop : Intrinsic<[], [], [IntrNoMem, IntrHasSideEffects]>;

}
