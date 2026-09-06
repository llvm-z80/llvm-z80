// RUN: not %clang_cc1 -triple msp430 -fsyntax-only %s 2>&1 | FileCheck %s
// RUN: not %clang_cc1 -triple z80 -fsyntax-only %s 2>&1 | FileCheck %s

// An array completed from its initializer list must obey the same size
// limit as one with an explicit bound; it used to sail past the check and
// crash CodeGen on targets with a 16-bit size_t.

struct S { char c[4096]; };

// 17 * 4096 bytes does not fit in a 16-bit address space.
// CHECK: error: array is too large (17 elements)
struct S overflowing[] = {
    {{0}}, {{0}}, {{0}}, {{0}}, {{0}}, {{0}}, {{0}}, {{0}}, {{0}},
    {{0}}, {{0}}, {{0}}, {{0}}, {{0}}, {{0}}, {{0}}, {{0}},
};

// 15 elements still fit.
// CHECK-NOT: fitting
struct S fitting[] = {
    {{0}}, {{0}}, {{0}}, {{0}}, {{0}}, {{0}}, {{0}}, {{0}}, {{0}},
    {{0}}, {{0}}, {{0}}, {{0}}, {{0}}, {{0}},
};
