// RUN: %clang_cc1 -triple z80 -fsyntax-only -verify %s

// z80_smallc / z80_callee / z80_fastcall / sdcccall / z80_allreg are DISTINCT
// calling conventions; applying two CONFLICTING ones to one function must be
// diagnosed (like every other target's CC attributes), not silently collapsed
// to whichever is applied last.  ravn/llvm-z80#281.
//
// The exception is the pair that lives on ORTHOGONAL ABI axes: z80_smallc
// (left-to-right argument order) + z80_callee (callee stack cleanup) compose
// into the z88dk `__smallc __z88dk_callee` convention rather than conflicting.
// ravn/llvm-z80#282.  (Codegen for the composed convention is checked in
// CodeGen/z80-smallc-callee.c.)

void sc_callee(int a, int b)
    __attribute__((z80_smallc)) __attribute__((z80_callee)); // ok: composes (#282)

void callee_sc(int a, int b)
    __attribute__((z80_callee)) __attribute__((z80_smallc)); // ok: composes, order-independent (#282)

void callee_fast(int a, int b)
    __attribute__((z80_callee)) __attribute__((z80_fastcall)); // expected-error {{are not compatible}}

void smallc_sdcc0(int a, int b)
    __attribute__((z80_smallc)) __attribute__((sdcccall(0))); // expected-error {{are not compatible}}

void allreg_callee(int a, int b)
    __attribute__((z80_allreg)) __attribute__((z80_callee)); // expected-error {{are not compatible}}

// A single CC attribute is fine.
void just_smallc(int a, int b) __attribute__((z80_smallc)); // ok
void just_callee(int a, int b) __attribute__((z80_callee)); // ok
void just_fast(int a) __attribute__((z80_fastcall));        // ok

// Repeating the SAME convention is not a conflict.
void same_twice(int a, int b)
    __attribute__((z80_callee)) __attribute__((z80_callee)); // ok
