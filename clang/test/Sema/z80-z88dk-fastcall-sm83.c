// RUN: %clang_cc1 -triple sm83 -fsyntax-only -verify %s

// z88dk_fastcall is implemented for the Z80 target, but its SM83 ABI has not
// been specified and validated yet. It must not reach SM83 call lowering.
// expected-warning@+1 {{'z88dk_fastcall' calling convention is not supported for this target}}
void sm83_fastcall(int value) __attribute__((z88dk_fastcall));
