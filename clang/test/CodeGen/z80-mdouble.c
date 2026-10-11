// RUN: %clang_cc1 -triple z80  -emit-llvm -o - %s | FileCheck --check-prefix=CHECK-32 %s
// RUN: %clang_cc1 -triple z80  -mdouble=64 -emit-llvm -o - %s | FileCheck --check-prefix=CHECK-64 %s
// RUN: %clang_cc1 -triple z80  -mdouble=32 -emit-llvm -o - %s | FileCheck --check-prefix=CHECK-32 %s
// RUN: %clang_cc1 -triple sm83 -emit-llvm -o - %s | FileCheck --check-prefix=CHECK-32 %s
// RUN: %clang_cc1 -triple sm83 -mdouble=64 -emit-llvm -o - %s | FileCheck --check-prefix=CHECK-64 %s
// RUN: %clang_cc1 -triple sm83 -mdouble=32 -emit-llvm -o - %s | FileCheck --check-prefix=CHECK-32 %s
//
// By default, `double` and `long double` are IEEE-754 binary32 on Z80 and SM83,
// the same width and bit format as `float`. -mdouble=64 makes them binary64.

// CHECK-64: define{{.*}} double @add(double noundef %a, double noundef %b)
// CHECK-64: fadd double
// CHECK-32: define{{.*}} float @add(float noundef %a, float noundef %b)
// CHECK-32: fadd float
double add(double a, double b) {
  return a + b;
}

// CHECK-64: define{{.*}} i16 @dsize()
// CHECK-64: ret i16 8
// CHECK-32: define{{.*}} i16 @dsize()
// CHECK-32: ret i16 4
int dsize(void) {
  return (int)sizeof(double);
}

// CHECK-64: define{{.*}} i16 @ldsize()
// CHECK-64: ret i16 8
// CHECK-32: define{{.*}} i16 @ldsize()
// CHECK-32: ret i16 4
int ldsize(void) {
  return (int)sizeof(long double);
}
