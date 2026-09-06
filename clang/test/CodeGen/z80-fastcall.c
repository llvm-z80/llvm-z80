// RUN: %clang_cc1 -triple z80 -emit-llvm -o - %s | FileCheck %s
//
// ravn/llvm-z80: __attribute__((z80_fastcall)) selects the z88dk
// __z88dk_fastcall calling convention (CallingConv::Z80_Z88dkFastCall = 130).
// A single argument is passed in a fixed register by width -- i8 in L, i16 in
// HL, i32 in DE:HL (DE high, HL low) -- and the return value uses the same
// registers.  This test verifies the FRONTEND mapping: clang lowers the
// attribute to `cc130` on both the definition and the call site.  The actual
// register discipline (L / HL / DE:HL) is pinned by the backend lit test
// llvm/test/CodeGen/Z80/z88dk-fastcall.ll.

// CHECK: define{{.*}}cc130 {{.*}}i8 @id8(i8
__attribute__((z80_fastcall)) unsigned char id8(unsigned char x) { return x; }

// CHECK: define{{.*}}cc130 {{.*}}i16 @id16(i16
__attribute__((z80_fastcall)) unsigned short id16(unsigned short x) { return x; }

// CHECK: define{{.*}}cc130 i32 @id32(i32
__attribute__((z80_fastcall)) unsigned long id32(unsigned long x) { return x; }

// The call site carries the same convention.
// CHECK-LABEL: @call_id8(
// CHECK: call cc130 {{.*}}i8 @id8(i8 noundef zeroext 17)
unsigned char call_id8(void) { return id8(17); }
