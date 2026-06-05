; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O0 < %s | FileCheck %s --check-prefix=CHECK-O0
; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s --check-prefix=CHECK-O1

; Fibonacci function - tests register allocation with spills.
; At -O0 the ADD result must be correctly spilled when HL is reused
; for the equality comparison.

define i16 @fib(i16 %n) {
; CHECK-O0-LABEL: _fib:
; The loop body: ADD for fibonacci, then fused EQ compare-and-branch
; CMP+BR fusion: G_ICMP EQ + G_BRCOND → XOR-based compare + JP Z
; CHECK-O0:       add hl,de
; CHECK-O0:       xor
; CHECK-O0:       or
; CHECK-O0:       jp z,

; CHECK-O1-LABEL: _fib:
; CHECK-O1:       add hl,de
  %cmp = icmp sgt i16 %n, 0
  br i1 %cmp, label %loop, label %exit

exit:
  %result = phi i16 [ 0, %0 ], [ %curr, %loop ]
  ret i16 %result

loop:
  %prev = phi i16 [ %curr, %loop ], [ 0, %0 ]
  %counter = phi i16 [ %next_counter, %loop ], [ 0, %0 ]
  %curr = phi i16 [ %sum, %loop ], [ 1, %0 ]
  %sum = add nsw i16 %prev, %curr
  %next_counter = add nuw nsw i16 %counter, 1
  %done = icmp eq i16 %next_counter, %n
  br i1 %done, label %exit, label %loop
}
