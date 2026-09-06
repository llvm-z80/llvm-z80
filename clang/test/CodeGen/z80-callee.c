// RUN: %clang_cc1 -triple z80 -emit-llvm -o - %s | FileCheck %s
//
// ravn/llvm-z80: __attribute__((z80_callee)) selects the z88dk __z88dk_callee
// calling convention (CallingConv::Z80_Z88dkCallee = 131).  Arguments are
// pushed on the stack like __sdcccall(0), but the CALLEE cleans them up on
// return; the return value uses the z88dk classic registers (i8 in L, i16 in
// HL, i32 in DE:HL).  This test verifies the FRONTEND mapping: clang lowers the
// attribute to `cc131` on both the definition and the call site.  The actual
// stack-cleanup discipline (callee pops, caller does not) is pinned by the
// backend lit test llvm/test/CodeGen/Z80/z88dk-callee.ll.

// CHECK: define{{.*}}cc131 void @sink2(i16
__attribute__((z80_callee)) void sink2(unsigned short a, unsigned short b);
__attribute__((z80_callee)) void sink2(unsigned short a, unsigned short b) {
  (void)a;
  (void)b;
}

// CHECK: define{{.*}}cc131 {{.*}}i16 @add2(i16
__attribute__((z80_callee)) unsigned short add2(unsigned short a,
                                                unsigned short b) {
  return a + b;
}

// The call site carries the same convention.
// CHECK-LABEL: @call_sink2(
// CHECK: call cc131 void @sink2(i16 noundef {{(zeroext )?}}1, i16 noundef {{(zeroext )?}}2)
void call_sink2(void) { sink2(1, 2); }
