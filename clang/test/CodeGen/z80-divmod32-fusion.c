// RUN: %clang_cc1 -triple z80 -O1 -S -mllvm -z80-asm-format=sdasz80 -o - %s | FileCheck %s
//
// When both the quotient (a/b) and the remainder (a%b) of the same i32
// division are live, the legalizer fuses them into ONE runtime call
// (__udivmodsi4 / __divmodsi4) instead of two separate __udivsi3 + __umodsi3.
// This test confirms the C -> IR -> asm pipeline triggers the fusion.
//
// NOTE: unsigned long is i32 on this target.

unsigned long udivmod32(unsigned long a, unsigned long b) {
  return a / b + a % b;
}
// CHECK-LABEL: _udivmod32:
// CHECK:       call ___udivmodsi4
// CHECK-NOT:   ___udivsi3
// CHECK-NOT:   ___umodsi3

long sdivmod32(long a, long b) {
  return a / b + a % b;
}
// CHECK-LABEL: _sdivmod32:
// CHECK:       call ___divmodsi4
// CHECK-NOT:   ___divsi3
// CHECK-NOT:   ___modsi3
