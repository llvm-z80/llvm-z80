// RUN: %clang_cc1 -triple z80 -fsyntax-only -verify %s
//
// What each of the Z80 conventions refuses, as opposed to how they compose
// with one another (that is z80-conflicting-callconv.c).

#define SC   __attribute__((smallc))
#define ZC   __attribute__((z88dk_callee))
#define ZF   __attribute__((z88dk_fastcall))
#define C0   __attribute__((sdcccall(0)))

struct Pair { unsigned short a, b; };

// __z88dk_fastcall carries its single argument in L, HL or DE:HL and has no
// stack to fall back on, so anything that does not reach those registers has
// to be rejected rather than quietly pushed.

void fc_ok8(unsigned char x) ZF;   // ok
void fc_ok16(unsigned short x) ZF; // ok
void fc_ok32(unsigned long x) ZF;  // ok
unsigned long fc_ret32(unsigned short x) ZF; // ok, returns in DE:HL

// expected-error@+1 {{z88dk_fastcall function must be declared with exactly one parameter}}
void fc_two(unsigned short a, unsigned short b) ZF;
// expected-error@+1 {{z88dk_fastcall function must be declared with exactly one parameter}}
void fc_none(void) ZF;
// expected-error@+1 {{z88dk_fastcall function must be declared with exactly one parameter}}
void fc_varargs(unsigned short a, ...) ZF;

// expected-error@+1 {{z88dk_fastcall parameter type must fit in registers: a scalar no wider than 32 bits}}
void fc_wide(unsigned long long x) ZF;
// expected-error@+1 {{z88dk_fastcall parameter type must fit in registers: a scalar no wider than 32 bits}}
void fc_aggregate(struct Pair p) ZF;
// A struct return becomes a hidden pointer, which is a second argument.
// expected-error@+1 {{z88dk_fastcall return type must fit in registers: a scalar no wider than 32 bits}}
struct Pair fc_ret_aggregate(unsigned short x) ZF;

// A callee that pops its own arguments cannot pop a count only the caller
// knows, so __z88dk_callee rules out varargs whatever base it sits on.  The
// two __smallc spellings are refused for a different reason: a left-to-right
// push leaves a variadic callee no way to find where the fixed arguments end.

// expected-error@+1 {{variadic function cannot use z88dk_callee calling convention}}
void zc_varargs(unsigned short a, ...) ZC;
// expected-error@+1 {{variadic function cannot use sdcccall(0) z88dk_callee calling convention}}
void c0c_varargs(unsigned short a, ...) C0 ZC;
// expected-error@+1 {{variadic function cannot use smallc calling convention}}
void sc_varargs(unsigned short a, ...) SC;
// Both attributes refuse varargs, and each is diagnosed as it is applied.
// expected-error@+2 {{variadic function cannot use smallc calling convention}}
// expected-error@+1 {{variadic function cannot use z88dk_callee calling convention}}
void scc_varargs(unsigned short a, ...) SC ZC;

// Plain __sdcccall(0) is caller-cleanup, so varargs are fine there.
void c0_varargs(unsigned short a, ...) C0; // ok

// The conventions apply to function types, not to objects.
// expected-warning@+1 {{'smallc' only applies to function types; type here is 'int'}}
SC int not_a_function;
