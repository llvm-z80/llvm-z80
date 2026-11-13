/*===---- intrinsic.h - Z80 privileged-instruction intrinsics --------------===
 *
 * Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 *===-----------------------------------------------------------------------===
 *
 * Clang ships this header for the Z80/SM83 targets so that the *same* source
 * compiles unchanged with both clang and SDCC/z88dk: `#include <intrinsic.h>`
 * resolves to this file under clang and to z88dk's <intrinsic.h> under SDCC,
 * exposing the identical `intrinsic_*` API.  No per-source #ifdef is needed.
 *
 * Each wrapper lowers to the matching __builtin_z80_* (a side-effecting
 * llvm.z80.* intrinsic), so the clang path carries NO inline assembly and the
 * optimizer models the side effects precisely.  ravn/llvm-z80#42.
 *
 * The names and signatures mirror z88dk's <intrinsic.h> exactly so a program
 * may use either toolchain.  Only the subset that maps to a real Z80/SM83
 * instruction is provided here.
 *
 *===-----------------------------------------------------------------------===
 */
#ifndef __INTRINSIC_H__
#define __INTRINSIC_H__

#if !defined(__z80__) && !defined(__sm83__)
#error "<intrinsic.h> is only available on Z80/SM83 targets"
#endif

/* Disable interrupts (DI). */
static __inline__ void intrinsic_di(void) { __builtin_z80_di(); }

/* Enable interrupts (EI). */
static __inline__ void intrinsic_ei(void) { __builtin_z80_ei(); }

/* Halt the CPU until the next interrupt (HALT). */
static __inline__ void intrinsic_halt(void) { __builtin_z80_halt(); }

/* No operation (NOP). */
static __inline__ void intrinsic_nop(void) { __builtin_z80_nop(); }

#if defined(__z80__)
/* Select interrupt mode 2 (IM 2).  Z80 only -- SM83 has no IM instruction. */
static __inline__ void intrinsic_im_2(void) { __builtin_z80_im2(); }
#endif

#endif /* __INTRINSIC_H__ */
