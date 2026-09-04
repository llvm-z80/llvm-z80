; RUN: llc -mtriple=z80 -O2 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=z80 -O2 -stop-after=z80-late-opt < %s | FileCheck %s --check-prefix=MIR

; XOR A zeroes the accumulator whatever it held, so it must not be described
; as reading it: the zeroing lands wherever a zero is wanted, including on an
; A that nothing has written.

; CHECK-LABEL: f:
; CHECK: xor a
; MIR-NOT: XOR_A{{.*}}implicit $a
define i8 @f(ptr %p, i8 %n) {
entry:
  %c = icmp eq i8 %n, 0
  br i1 %c, label %zero, label %load
zero:
  ret i8 0
load:
  %v = load i8, ptr %p
  ret i8 %v
}
