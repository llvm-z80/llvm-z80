// RUN: %clang_cc1 -triple z80 -emit-llvm -o - %s | FileCheck %s
//
// ravn/llvm-z80#42 — the __builtin_z80_* privileged-instruction builtins must
// lower to their side-effecting llvm.z80.* intrinsics so C code can emit DI/EI/
// HALT/NOP/IM 2/LD I,A without inline assembly.

// CHECK-LABEL: define {{.*}}void @test_di()
// CHECK: call void @llvm.z80.di()
void test_di(void) { __builtin_z80_di(); }

// CHECK-LABEL: define {{.*}}void @test_ei()
// CHECK: call void @llvm.z80.ei()
void test_ei(void) { __builtin_z80_ei(); }

// CHECK-LABEL: define {{.*}}void @test_halt()
// CHECK: call void @llvm.z80.halt()
void test_halt(void) { __builtin_z80_halt(); }

// CHECK-LABEL: define {{.*}}void @test_nop()
// CHECK: call void @llvm.z80.nop()
void test_nop(void) { __builtin_z80_nop(); }

// CHECK-LABEL: define {{.*}}void @test_im2()
// CHECK: call void @llvm.z80.im2()
void test_im2(void) { __builtin_z80_im2(); }

// CHECK-LABEL: define {{.*}}void @test_set_i(
// CHECK: call void @llvm.z80.set.i(i8
void test_set_i(unsigned char v) { __builtin_z80_set_i(v); }
