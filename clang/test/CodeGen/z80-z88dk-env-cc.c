// The z88dk environment uses __sdcccall(0) for C.  There __sdcccall(0) is plain
// C, and __sdcccall(1) is cc240.
// RUN: %clang_cc1 -triple z80-unknown-none-z88dk -DZ88DK -emit-llvm -o - %s | FileCheck %s --check-prefixes=CHECK,Z88DK
// RUN: %clang_cc1 -triple z80 -emit-llvm -o - %s | FileCheck %s --check-prefixes=CHECK,SDCC

// CHECK: define dso_local i16 @plain(
int plain(int a, int b) { return a + b; }

// Z88DK: define dso_local i16 @zero(
// SDCC: define dso_local z80_sdcccall0 i16 @zero(
__attribute__((sdcccall(0))) int zero(int a, int b) { return a + b; }

// Z88DK: define dso_local cc240 i16 @one(
// SDCC: define dso_local i16 @one(
__attribute__((sdcccall(1))) int one(int a, int b) { return a + b; }

// __z88dk_callee on its own modifies the environment's C convention.
// Z88DK: define dso_local cc245 i16 @callee(
// SDCC: define dso_local cc244 i16 @callee(
__attribute__((z88dk_callee)) int callee(int a, int b) { return a + b; }

// CHECK: define dso_local cc244 i16 @one_callee(
__attribute__((sdcccall(1))) __attribute__((z88dk_callee))
int one_callee(int a, int b) { return a + b; }

// CHECK: define dso_local cc244 i16 @callee_one(
__attribute__((z88dk_callee)) __attribute__((sdcccall(1)))
int callee_one(int a, int b) { return a + b; }

// CHECK: define dso_local cc245 i16 @zero_callee(
__attribute__((sdcccall(0))) __attribute__((z88dk_callee))
int zero_callee(int a, int b) { return a + b; }

// CHECK-LABEL: @calls(
// Z88DK: call cc240 i16 @one(
// SDCC: call i16 @one(
int calls(void) { return one(1, 2); }

#ifdef Z88DK
// __sdcccall(0) is plain C here, so the two function types match.
int (*to_zero)(int, int) = zero;
#endif
