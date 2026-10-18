/*===- TicksGlue.c - Connect the ticks emulator core to z80-run ---*- C -*-===*\
|*                                                                            *|
|* Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.      *|
|* See https://llvm.org/LICENSE.txt for license information.                  *|
|* SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception                    *|
|*                                                                            *|
|*===----------------------------------------------------------------------===*|
|*                                                                            *|
|* z80-run builds only the CPU core of z88dk-ticks, ticks.c and the per-CPU   *|
|* instruction files. This file defines the functions from the rest of ticks  *|
|* that the core calls.                                                       *|
|*                                                                            *|
\*===----------------------------------------------------------------------===*/

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "TicksGlue.h"

/* z80-run reports every outcome itself, so the core's own messages are
 * dropped. */
static int discardPrintf(const char *Format, ...) {
  (void)Format;
  return 0;
}
static int discardFprintf(FILE *Stream, const char *Format, ...) {
  (void)Stream;
  (void)Format;
  return 0;
}
#define printf discardPrintf
#define fprintf discardFprintf
/* Included so the memory accessors and the hook below are inlined into the
 * interpreter loop. */
#include "ticks.c"
#undef printf
#undef fprintf

int c_cpu = CPU_Z80;
int c_z80asm_tests = 0;
int profiler_enabled = 0;

static uint8_t Memory[0x10000];

static long long Budget;
static int StopRequested;
static Z80RunResult Stopped;

/* Ends the run after the current instruction by using up the cycle budget.
 * cpu_run stops only at the end address or at the end of the budget. */
static void requestStop(Z80RunStop Why, uint16_t At) {
  if (StopRequested)
    return;
  StopRequested = 1;
  Stopped.Stop = Why;
  Stopped.Cycles = (uint64_t)st;
  Stopped.PC = At;
  st = Budget;
}

static uint16_t read16(uint16_t Addr) {
  return Memory[Addr] | Memory[(uint16_t)(Addr + 1)] << 8;
}

static int locksUpSM83(uint8_t Op) {
  switch (Op) {
  case 0xD3:
  case 0xDB:
  case 0xDD:
  case 0xE3:
  case 0xE4:
  case 0xEB:
  case 0xEC:
  case 0xED:
  case 0xF4:
  case 0xFC:
  case 0xFD:
    return 1;
  default:
    return 0;
  }
}

/* A DD or FD prefix in front of HALT changes nothing on the Z80, so the pair
 * is a HALT that starts at the prefix. */
static int isHalt(uint8_t Op) {
  if (Op == 0x76)
    return 1;
  return !isgbz80() && (Op == 0xDD || Op == 0xFD) &&
         Memory[(uint16_t)(pc + 1)] == 0x76;
}

/* CC is the condition field of a jump: NZ, Z, NC, C, PO, PE, P, M. */
static int conditionHolds(unsigned CC) {
  int F = f();
  int Zero = isgbz80() ? F & 0x80 : F & 0x40;
  int Carry = isgbz80() ? F & 0x10 : F & 0x01;
  switch (CC) {
  case 0:
    return !Zero;
  case 1:
    return Zero != 0;
  case 2:
    return !Carry;
  case 3:
    return Carry != 0;
  case 4:
    return !(F & 0x04);
  case 5:
    return (F & 0x04) != 0;
  case 6:
    return !(F & 0x80);
  default:
    return (F & 0x80) != 0;
  }
}

/* Whether the instruction at PC jumps to itself. A jump changes neither the
 * flags nor the registers, and no interrupt is raised, so such a jump repeats
 * forever. */
static int jumpsToItself(uint8_t Op) {
  uint8_t Next = Memory[(uint16_t)(pc + 1)];
  if (Op == 0x18)
    return Next == 0xFE;
  if ((Op & 0xE7) == 0x20)
    return Next == 0xFE && conditionHolds((Op >> 3) & 3);
  if (Op == 0xC3)
    return read16(pc + 1) == pc;
  /* The SM83 has only the first four conditions. */
  if ((Op & 0xC7) == 0xC2 && (!isgbz80() || Op < 0xE0))
    return read16(pc + 1) == pc && conditionHolds((Op >> 3) & 7);
  if (Op == 0xE9)
    return z80RunReg(Z80RunRegHL) == pc;
  if (!isgbz80() && Next == 0xE9 && (Op == 0xDD || Op == 0xFD))
    return z80RunReg(Op == 0xDD ? Z80RunRegIX : Z80RunRegIY) == pc;
  return 0;
}

/* cpu_run calls this at every instruction boundary, before the fetch. */
void debugger(void) {
  uint8_t Op = Memory[pc];
  if (isHalt(Op))
    requestStop(Z80RunStopHalt, pc);
  else if (isgbz80() && locksUpSM83(Op))
    requestStop(Z80RunStopIllegal, pc);
  else if (jumpsToItself(Op))
    requestStop(Z80RunStopHang, pc);
}

/* An ED FE trap. The core has already fetched both bytes. */
void PatchZ80(void) { requestStop(Z80RunStopTrap, pc - 2); }

uint8_t *get_memory_addr(uint32_t Addr, memtype Type) {
  (void)Type;
  return &Memory[Addr & 0xFFFF];
}

uint8_t get_memory(uint32_t Addr, memtype Type) {
  (void)Type;
  return Memory[Addr & 0xFFFF];
}

uint8_t put_memory(uint16_t Addr, uint8_t Value) {
  return Memory[Addr] = Value;
}

int hook_console_in(int Port) {
  (void)Port;
  return -1;
}
int hook_console_out(int Port, int Value) {
  (void)Port;
  (void)Value;
  return -1;
}
int acia_in(int Port) {
  (void)Port;
  return -1;
}
int acia_out(int Port, int Value) {
  (void)Port;
  (void)Value;
  return -1;
}
int apu_in(int Port) {
  (void)Port;
  return -1;
}
int apu_out(int Port, int Value) {
  (void)Port;
  (void)Value;
  return -1;
}
int memory_in(int Port) {
  (void)Port;
  return -1;
}
void memory_handle_paging(int Port, int Value) {
  (void)Port;
  (void)Value;
}
int rabbit_get_ioi_reg(int Reg) {
  (void)Reg;
  return 0;
}

void debugger_request_a_break(void) {}
void warn_existing_temp_breakpoints(void) {}
void profiler_stop(void) {}

void z80RunInit(Z80RunCPU CPU) {
  c_cpu = CPU == Z80RunCPUSM83 ? CPU_GBZ80 : CPU_Z80;
  memset(Memory, 0, sizeof(Memory));
}

uint8_t *z80RunMemory(void) { return Memory; }

void z80RunSetPC(uint16_t PC) { pc = PC; }

uint16_t z80RunReg(Z80RunReg Reg) {
  switch (Reg) {
  case Z80RunRegAF:
    return (uint16_t)(a << 8 | (f() & 0xFF));
  case Z80RunRegBC:
    return (uint16_t)(b << 8 | c);
  case Z80RunRegDE:
    return (uint16_t)(d << 8 | e);
  case Z80RunRegHL:
    return (uint16_t)(h << 8 | l);
  case Z80RunRegIX:
    return (uint16_t)(xh << 8 | xl);
  case Z80RunRegIY:
    return (uint16_t)(yh << 8 | yl);
  case Z80RunRegSP:
    return sp;
  case Z80RunRegPC:
    return pc;
  }
  return 0;
}

Z80RunResult z80RunExecute(uint64_t MaxCycles) {
  Z80RunResult Result;
  /* Half the range of st, a signed long long, leaves room for the cycles that
   * the stopping instruction adds. */
  Budget = MaxCycles > LLONG_MAX / 2 ? LLONG_MAX / 2 : (long long)MaxCycles;
  StopRequested = 0;
  /* A start and end address of -1 match no PC, so the count never restarts
   * and only the budget or a stop request ends the loop. */
  cpu_run(Budget, 0, 0, -1, -1);
  if (StopRequested) {
    st = (long long)Stopped.Cycles;
    return Stopped;
  }
  Result.Stop = Z80RunStopBudget;
  Result.Cycles = (uint64_t)st;
  Result.PC = pc;
  return Result;
}
