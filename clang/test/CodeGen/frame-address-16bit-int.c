// RUN: %clang_cc1 -triple msp430 -emit-llvm %s -o - | FileCheck %s
// RUN: %clang_cc1 -triple avr -emit-llvm %s -o - | FileCheck %s
// RUN: %clang_cc1 -triple z80 -emit-llvm %s -o - | FileCheck %s

// The depth operand of these intrinsics is fixed at i32; on targets where
// 'int' is 16 bits the constant argument must still be emitted as i32.

void *f(void) {
  return __builtin_frame_address(0);
  // CHECK: @llvm.frameaddress.p0(i32 0)
}

void *g(void) {
  return __builtin_return_address(1);
  // CHECK: @llvm.returnaddress{{[^(]*}}(i32 1)
}
