// RUN: %clang_cc1 -triple z80 -emit-llvm -o - %s | FileCheck %s
//
// __attribute__((z88dk_callee)) on its own selects CallingConv::Z80_Z88dkCallee
// = 131.  In SDCC the keyword is a MODIFIER over whichever __sdcccall level is
// in effect rather than a convention of its own, so on its own it keeps
// __sdcccall(1) argument passing and return registers and only moves stack
// cleanup to the callee.  Writing __attribute__((sdcccall(0))) alongside it
// selects cc132 instead.  This test verifies the FRONTEND mapping; the actual
// stack-cleanup discipline is pinned by the backend lit test
// llvm/test/CodeGen/Z80/z88dk-callee.ll.

// CHECK: define{{.*}}cc131 void @sink2(i16
__attribute__((z88dk_callee)) void sink2(unsigned short a, unsigned short b);
__attribute__((z88dk_callee)) void sink2(unsigned short a, unsigned short b) {
  (void)a;
  (void)b;
}

// CHECK: define{{.*}}cc131 {{.*}}i16 @ordered2(i16
__attribute__((z88dk_callee)) unsigned short ordered2(unsigned short a,
                                                unsigned short b) {
  return 10 * a + b;
}

// The call site carries the same convention.
// CHECK-LABEL: @call_sink2(
// CHECK: call cc131 void @sink2(i16 noundef {{(zeroext )?}}1, i16 noundef {{(zeroext )?}}2)
void call_sink2(void) { sink2(1, 2); }

// The modifier composes with an explicit base: sdcccall(0) + z88dk_callee is
// the all-on-the-stack variant, cc132.
// CHECK: define{{.*}}cc132 void @sink2_stack(i16
__attribute__((sdcccall(0))) __attribute__((z88dk_callee))
void sink2_stack(unsigned short a, unsigned short b) { (void)a; (void)b; }
