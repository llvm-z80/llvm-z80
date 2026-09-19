; RUN: llc -mtriple=z80 -O1 < %s | FileCheck %s

; Test: loop counter placed in B is folded to DJNZ.

; CHECK-LABEL: _delay:
; CHECK:      	ld	b,{{ *}}a
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

@sink8 = external global i8

; Test: C countdown loop 'do { sink8 = 0; } while (--n)' clobbers A in the body
; and produces 'LD A, B; DEC A; LD B, A; OR A; JR NZ', which folds to DJNZ.
; CHECK-LABEL: _countdown_with_body:
; CHECK:      	ld	b,{{ *}}a
; CHECK:      .LBB1_1:
; CHECK:      	xor	a
; CHECK:      	ld	(de),{{ *}}a
; CHECK-NEXT: 	djnz	[[LOOP:\.LBB[0-9_]+]]
; CHECK:      	ret
define void @countdown_with_body(i8 %n) {
entry:
  br label %loop

loop:
  %i = phi i8 [ %n, %entry ], [ %i.next, %loop ]
  store volatile i8 0, ptr @sink8, align 1
  %i.next = add i8 %i, -1
  %cond = icmp eq i8 %i.next, 0
  br i1 %cond, label %exit, label %loop

exit:
  ret void
}
