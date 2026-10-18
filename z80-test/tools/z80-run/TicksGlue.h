/*===- TicksGlue.h - C interface to the ticks emulator core -------*- C -*-===*\
|*                                                                            *|
|* Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.      *|
|* See https://llvm.org/LICENSE.txt for license information.                  *|
|* SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception                    *|
|*                                                                            *|
|*===----------------------------------------------------------------------===*|
|*                                                                            *|
|* The emulator core keeps the CPU registers in globals named a, b, pc, sp    *|
|* and so on, so only TicksGlue.c includes ticks.h. Everything else goes      *|
|* through this header.                                                       *|
|*                                                                            *|
\*===----------------------------------------------------------------------===*/

#ifndef Z80_TEST_TOOLS_Z80_RUN_TICKSGLUE_H
#define Z80_TEST_TOOLS_Z80_RUN_TICKSGLUE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  Z80RunCPUZ80,
  Z80RunCPUSM83,
} Z80RunCPU;

typedef enum {
  Z80RunStopHalt,
  Z80RunStopBudget,
  /* Jumped to itself. */
  Z80RunStopHang,
  /* Reached an opcode the SM83 locks up on. */
  Z80RunStopIllegal,
  /* Executed an ED FE trap. */
  Z80RunStopTrap,
} Z80RunStop;

typedef enum {
  Z80RunRegAF,
  Z80RunRegBC,
  Z80RunRegDE,
  Z80RunRegHL,
  Z80RunRegIX,
  Z80RunRegIY,
  Z80RunRegSP,
  Z80RunRegPC,
} Z80RunReg;

typedef struct {
  Z80RunStop Stop;
  /* T-states spent before the stopping instruction. */
  uint64_t Cycles;
  /* Address of the stopping instruction. */
  uint16_t PC;
} Z80RunResult;

/* The core keeps the CPU state in globals, so a process runs a single
 * program. */
void z80RunInit(Z80RunCPU CPU);

uint8_t *z80RunMemory(void);

void z80RunSetPC(uint16_t PC);

uint16_t z80RunReg(Z80RunReg Reg);

Z80RunResult z80RunExecute(uint64_t MaxCycles);

#ifdef __cplusplus
}
#endif

#endif
