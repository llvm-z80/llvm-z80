// REQUIRES: z80-registered-target
// RUN: %clang_cc1 -triple z80 -emit-llvm %s -o - | FileCheck %s
// RUN: %clang_cc1 -triple sm83 -emit-llvm %s -o - | FileCheck %s

// Aggregates are passed byval, so the caller copies the bytes into the
// argument area. va_arg must read them in place; emitting `va_arg ptr`
// would dereference struct bytes as an address.

struct pair { int a; int b; };

// CHECK-LABEL: @take
// CHECK-NOT: va_arg ptr {{.*}}, ptr
// CHECK: %argp.cur = load ptr, ptr %ap
// CHECK: call void @llvm.memcpy{{.*}}%argp.cur
// CHECK-NOT: va_arg ptr {{.*}}, ptr
// CHECK: va_arg ptr %ap, i16
int take(int n, ...) {
  __builtin_va_list ap;
  __builtin_va_start(ap, n);
  struct pair p = __builtin_va_arg(ap, struct pair);
  int tail = __builtin_va_arg(ap, int);
  __builtin_va_end(ap);
  return p.a + p.b + tail;
}

// The caller side stays byval.
// CHECK-LABEL: @call
// CHECK: call {{.*}} @take(i16 noundef 1, ptr noundef byval(%struct.pair)
int call(void) {
  struct pair p = {1, 2};
  return take(1, p, 3);
}
