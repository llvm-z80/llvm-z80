; RUN: llc -mtriple=z80 -O1 < %s | FileCheck %s

; Test: loop counter placed in B is folded to DJNZ.

; CHECK-LABEL: _delay:
; CHECK:      	ld	b, a
; CHECK:      	djnz	.LBB0_1
; CHECK:      	ret
define void @delay(i8 %n) {
entry:
  br label %loop

loop:
  %i = phi i8 [ %n, %entry ], [ %i.next, %loop ]
  %i.next = add i8 %i, -1
  %cond = icmp ne i8 %i.next, 0
  br i1 %cond, label %loop, label %exit

exit:
  ret void
}
