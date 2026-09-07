// RUN: %clang_cc1 -triple z80 -fsyntax-only -verify %s

// SDCC builds its Z80 calling conventions from an argument-passing base
// (__sdcccall(0), __smallc or __z88dk_fastcall, with __sdcccall(1) as the
// default) plus the orthogonal __z88dk_callee modifier, and it lets the
// keywords stack up freely on one declaration.  clang composes the same way.
// Codegen for the composed conventions is checked in
// CodeGen/z80-smallc-z88dk-callee.c and CodeGen/z80-z88dk-callee.c.

// The modifier composes with every base.
void callee_default(int a, int b) __attribute__((z88dk_callee));           // ok
void sdcc0_callee(int a, int b)
    __attribute__((sdcccall(0))) __attribute__((z88dk_callee));            // ok
void smallc_callee(int a, int b)
    __attribute__((smallc)) __attribute__((z88dk_callee));                 // ok

// Composition is order-independent.
void callee_smallc(int a, int b)
    __attribute__((z88dk_callee)) __attribute__((smallc));                 // ok

// SDCC resolves a clash between bases rather than rejecting it: smallc
// overrides whichever sdcccall level is in effect.
void smallc_sdcc0(int a, int b)
    __attribute__((smallc)) __attribute__((sdcccall(0)));                  // ok
void sdcc1_smallc(int a, int b)
    __attribute__((sdcccall(1))) __attribute__((smallc));                  // ok

// z88dk_fastcall passes nothing on the stack, so it overrides every other
// base and absorbs the modifier.
void fast_callee(int a)
    __attribute__((z88dk_fastcall)) __attribute__((z88dk_callee));         // ok
void fast_smallc(int a)
    __attribute__((z88dk_fastcall)) __attribute__((smallc));               // ok

// Two different __sdcccall levels genuinely contradict each other.  SDCC
// rejects this pair too ("multiple incompatible calling conventions").
void sdcc0_sdcc1(int a, int b)
    __attribute__((sdcccall(0))) __attribute__((sdcccall(1)));             // expected-error {{are not compatible}}

// A convention from another target shares no axis with these.
void stdcall_smallc(int a, int b)
    __attribute__((stdcall)) __attribute__((smallc));                      // expected-warning {{'stdcall' calling convention is not supported for this target}}

// A single attribute is fine, and repeating one is not a conflict.
void just_smallc(int a, int b) __attribute__((smallc));                    // ok
void just_fast(int a) __attribute__((z88dk_fastcall));                     // ok
void same_twice(int a, int b)
    __attribute__((z88dk_callee)) __attribute__((z88dk_callee));           // ok

// z88dk fastcall is single-argument by construction.  An unprototyped
// declaration has to be rejected too: a later prototyped redeclaration
// inherits the convention through decl merging without passing through this
// check again, so accepting one would leave the rule unenforceable.
void fast_too_many(int a, int b)
    __attribute__((z88dk_fastcall)); // expected-error {{z88dk_fastcall function must be declared with exactly one parameter}}
void fast_variadic(int a, ...)
    __attribute__((z88dk_fastcall)); // expected-error {{z88dk_fastcall function must be declared with exactly one parameter}}
void fast_noproto()
    __attribute__((z88dk_fastcall)); // expected-error {{z88dk_fastcall function must be declared with exactly one parameter}}

// __smallc pushes left-to-right, which leaves a variadic callee no way to find
// where its fixed arguments end, so the convention is refused there.
void smallc_variadic(int a, ...)
    __attribute__((smallc)); // expected-error {{variadic function cannot use smallc calling convention}}
// Each attribute is checked as it is applied, so a variadic function wearing
// two conventions that both refuse varargs is diagnosed once for each.
void smallc_callee_variadic(int a, ...)
    __attribute__((smallc)) __attribute__((z88dk_callee)); // expected-error {{variadic function cannot use smallc calling convention}} expected-error {{variadic function cannot use z88dk_callee calling convention}}
