// RUN: %clang_cc1 -triple msp430 -emit-llvm %s -o - | FileCheck %s
// RUN: %clang_cc1 -triple avr -emit-llvm %s -o - | FileCheck %s
// RUN: %clang_cc1 -triple z80 -emit-llvm %s -o - | FileCheck %s

// The prefetch intrinsic's hint operands are fixed at i32; on targets where
// 'int' is 16 bits the constant arguments must still be emitted as i32.

void f(char *p) {
  __builtin_prefetch(p, 0, 3);
  // CHECK: @llvm.prefetch.p0(ptr %{{.*}}, i32 0, i32 3, i32 1)
  __builtin_prefetch(p, 1);
  // CHECK: @llvm.prefetch.p0(ptr %{{.*}}, i32 1, i32 3, i32 1)
  __builtin_prefetch(p);
  // CHECK: @llvm.prefetch.p0(ptr %{{.*}}, i32 0, i32 3, i32 1)
}
