// RUN: %clang_cc1 -triple z80 -O1 -S -mllvm -z80-float-sdcccall0 -mllvm -z80-asm-format=sdasz80 -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple z80 -O1 -S -mllvm -z80-asm-format=sdasz80 -o - %s | FileCheck %s --check-prefix=DEFAULT
//
// With -z80-float-sdcccall0, f32 arithmetic/compare/conversion libcalls use
// the sdcccall(0) ABI: ALL arguments are pushed on the stack. The most
// visible symptom: single-arg conversions (int<->float) push HL (the
// argument) before the call. Without the flag, the argument stays in the HL
// register pair and there is no push-before-call.
//
// This test confirms the full C -> IR -> asm pipeline produces the right
// calling sequence. The IR -> asm part is also covered by the sibling .ll
// tests (issue-277-f32-*-sdcccall0.ll).

float fadd(float a, float b) { return a + b; }
// CHECK-LABEL:   _fadd:
// CHECK:         push hl
// CHECK:         call ___addsf3
// DEFAULT-LABEL: _fadd:
// DEFAULT:       call ___addsf3

int flt(float a, float b) { return a < b; }
// CHECK-LABEL:   _flt:
// CHECK:         push hl
// CHECK:         call ___cmpsf2
// DEFAULT-LABEL: _flt:
// DEFAULT:       call ___cmpsf2

// Single-arg: int<->float conversions. Without the flag there are no
// push-hl before the call; with it the argument is fully pushed.
int to_int(float a) { return (int)a; }
// CHECK-LABEL:   _to_int:
// CHECK:         push hl
// CHECK:         call ___fixsfsi
// DEFAULT-LABEL: _to_int:
// DEFAULT-NOT:   push hl
// DEFAULT:       call ___fixsfsi

float to_float(int a) { return (float)a; }
// CHECK-LABEL:   _to_float:
// CHECK:         push hl
// CHECK:         call ___floatsisf
// DEFAULT-LABEL: _to_float:
// DEFAULT-NOT:   push hl
// DEFAULT:       call ___floatsisf
