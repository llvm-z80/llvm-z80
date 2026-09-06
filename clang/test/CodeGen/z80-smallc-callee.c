// RUN: %clang_cc1 -triple z80 -emit-llvm -o - %s | FileCheck %s
//
// ravn/llvm-z80#282: __attribute__((z80_smallc)) and
// __attribute__((z80_callee)) live on ORTHOGONAL ABI axes -- argument order
// (left-to-right, from smallc) and stack cleanup (callee, from callee) -- so
// writing both on one function COMPOSES them into the z88dk
// `__smallc __z88dk_callee` convention instead of conflicting (#281).  clang
// lowers the composition to CallingConv::Z80_SmallCCallee = 133 (`cc133`) on
// both the definition and the call site.  The actual dual-axis stack discipline
// (left-to-right push + callee cleanup) is pinned by the backend lit test
// llvm/test/CodeGen/Z80/z80-smallc-callee.ll.

// CHECK: define{{.*}}cc133 void @sink2(i16
__attribute__((z80_smallc)) __attribute__((z80_callee))
void sink2(unsigned short a, unsigned short b);
__attribute__((z80_smallc)) __attribute__((z80_callee))
void sink2(unsigned short a, unsigned short b) {
  (void)a;
  (void)b;
}

// The composition is order-independent: callee-then-smallc yields the same CC.
// CHECK: define{{.*}}cc133 {{.*}}i16 @add2(i16
__attribute__((z80_callee)) __attribute__((z80_smallc))
unsigned short add2(unsigned short a, unsigned short b) {
  return a + b;
}

// The call site carries the same composed convention.
// CHECK-LABEL: @call_sink2(
// CHECK: call cc133 void @sink2(i16 noundef {{(zeroext )?}}1, i16 noundef {{(zeroext )?}}2)
void call_sink2(void) { sink2(1, 2); }

// A call THROUGH A FUNCTION POINTER must carry the convention too -- this is the
// reason the axes must live in the function type (indirect calls have no decl to
// consult).  qsort-style comparators declared this way are the motivating case.
typedef void (*sink_fp)(unsigned short, unsigned short)
    __attribute__((z80_smallc)) __attribute__((z80_callee));
// CHECK-LABEL: @call_via_ptr(
// CHECK: call cc133 void %{{.*}}(i16
void call_via_ptr(sink_fp f) { f(3, 4); }
