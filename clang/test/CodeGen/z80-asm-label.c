// RUN: %clang_cc1 -triple z80 -emit-llvm %s -o - | FileCheck %s
// RUN: %clang_cc1 -triple sm83 -emit-llvm %s -o - | FileCheck %s

// The data layout prefixes globals with an underscore, so the frontend user
// label prefix must be "_" as well: it keeps the mangler assertion happy and
// makes asm("name") declarations carry the \01 exact-spelling marker so the
// renamed symbol is emitted verbatim instead of picking up the underscore.

extern int renamed asm("exact_name");
int plain;

int f(void) { return renamed + plain; }

// CHECK-DAG: @"\01exact_name" = external global i16
// CHECK-DAG: @plain = {{(dso_local )?}}global i16 0
