//===-- Z80CallingConv.td - Calling Conventions Z80 --------*- tablegen -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This describes the calling conventions for the Z80 architecture.
//
//===----------------------------------------------------------------------===//
//
// Z80 Calling Convention Notes (SDCC __sdcccall(1) compatible):
//
// The Z80 has limited registers compared to modern architectures:
// - 8-bit: A, B, C, D, E, H, L (7 general purpose)
// - 16-bit pairs: BC, DE, HL, IX, IY, SP
// - Flags: F register (not directly accessible)
//
// Register usage:
// - A: Accumulator, first 8-bit parameter and 8-bit return value
// - HL: Primary 16-bit register, first 16-bit parameter
// - DE: Second 16-bit parameter and 16-bit return value
// - IX: Frame pointer (callee-saved)
// - SP: Stack pointer (callee-saved)
//
// Caller-saved: A, F, B, C, D, E, H, L
// Callee-saved: IX
//
// Arguments (max 2 register params):
//   1st i8  -> A,    2nd i8  -> L,     2nd i16 -> DE
//   1st i16 -> HL,   2nd i16 -> DE,    2nd i8  -> stack
//   1st i32 -> HLDE, 2nd any -> stack
//   3rd+ always on stack (right to left push order)
//   Arg positions are consumed regardless of register assignment success.
//
// Stack argument packing:
//   i8 args are packed as 1 byte (push af; inc sp), NOT 2-byte aligned.
//   i16/i32/i64 args are naturally sized (2/4/8 bytes).
//
// Return values:
// - 8-bit:  A
// - 16-bit: DE
// - 32-bit: HLDE (HL high, DE low)
// - >32-bit: via sret pointer (hidden stack argument, see below)
//
// Sret (structure return) convention:
//   When the return type exceeds 32 bits (e.g., i64), the caller allocates
//   a buffer and pushes its address on the stack as a hidden first argument.
//   The sret pointer does NOT consume a register slot — regular arguments
//   still use HL/DE normally. The sret pointer is at the lowest stack
//   address (pushed last by caller, first thing after return address in
//   callee's frame).
//
// Callee-cleanup rules:
//   Non-variadic functions clean up their own stack arguments (including
//   sret pointer) when: return <= 16 bits, OR return is void (sret case),
//   OR (return float AND first arg float). Variadic = caller-cleanup.
//
//===----------------------------------------------------------------------===//

// Callee-saved registers: IX only (SDCC __sdcccall(1))
def Z80_CSR : CalleeSavedRegs<(add IX)>;

// Interrupt handler saves all register pairs (push/pop only works on pairs)
def Z80_Interrupt_CSR : CalleeSavedRegs<(add AF, BC, DE, HL, IX, IY)>;
