//===-- SM83CallingConv.td - Calling Conventions SM83 ------*- tablegen -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This describes the calling conventions for the SM83 (Game Boy CPU).
//
//===----------------------------------------------------------------------===//
//
// SM83 Calling Convention Notes (SDCC __sdcccall(1) compatible):
//
// SM83 differs from Z80: HL is reserved for memory access and never
// used for argument/return passing.
//
// Register usage:
// - A: Accumulator, first 8-bit parameter and 8-bit return value
// - DE: First 16-bit parameter
// - BC: Second 16-bit parameter and 16-bit return value
// - HL: Memory access only (not used for args/returns)
// - SP: Stack pointer
//
// Caller-saved: A, F, B, C, D, E, H, L (all GPRs)
// Callee-saved: none
//
// Arguments (max 2 register params):
//   1st i8  -> A,    2nd i8  -> E,     2nd i16 -> DE
//   1st i16 -> DE,   2nd i8  -> A,     2nd i16 -> BC
//   1st i32 -> DEBC, 2nd any -> stack
//   3rd+ always on stack (right to left push order)
//   Arg positions are consumed regardless of register assignment success.
//
// Stack argument packing:
//   i8 args are packed as 1 byte (push af; inc sp), NOT 2-byte aligned.
//   i16/i32/i64 args are naturally sized (2/4/8 bytes).
//
// Return values:
// - 8-bit:  A
// - 16-bit: BC
// - 32-bit: DEBC (DE high, BC low)
// - >32-bit: via sret pointer (hidden stack argument, see below)
//
// Sret (structure return) convention:
//   When the return type exceeds 32 bits (e.g., i64), the caller allocates
//   a buffer and pushes its address on the stack as a hidden first argument.
//   The sret pointer does NOT consume a register slot — regular arguments
//   still use DE/BC normally. The sret pointer is at the lowest stack
//   address (pushed last by caller, first thing after return address in
//   callee's frame).
//
// Callee-cleanup rules:
//   Non-variadic functions always use callee-cleanup (SM83 has no
//   conditional callee-cleanup based on return type, unlike Z80).
//
//===----------------------------------------------------------------------===//

// SM83 SDCC __sdcccall(1): no callee-saved registers.
// SM83 lacks IX/IY entirely; all GPRs are caller-saved.
def SM83_CSR : CalleeSavedRegs<(add)>;

// SM83 interrupt handler saves all register pairs (no IX/IY on SM83)
def SM83_Interrupt_CSR : CalleeSavedRegs<(add AF, BC, DE, HL)>;
