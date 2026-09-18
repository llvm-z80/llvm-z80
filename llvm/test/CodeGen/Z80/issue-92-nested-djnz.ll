; RUN: llc -mtriple=z80 < %s | FileCheck %s

; Issue #92: in nested 8-bit countdown loops, the INNER loop counter must get
; B (DJNZ-eligible). Before the fix, the outer counter took B and the inner
; emitted "dec r; jr nz" (3 extra bytes per inner iter).
;
; Distinguishing signal: the inner counter's dec/jr_nz lives in a self-looping
; MBB; the outer's dec/jr_nz lives in the latch MBB which branches to the
; outer header (a different MBB).

@port = external global ptr

; The outer loop counter must use a non-B register (e.g. C), and the inner
; loop counter must use B and emit DJNZ.
; CHECK-LABEL: _nested_djnz:
; CHECK:      	ld	c,{{ *}}a
; CHECK:      	djnz	[[INNER:\.LBB[0-9_]+]]
; CHECK:      	dec	c
; CHECK:      	ret
define void @nested_djnz(i8 zeroext %m, i8 zeroext %n) {
entry:
  br label %outer

outer:
  %o = phi i8 [ %m, %entry ], [ %o.next, %outer.latch ]
  br label %inner

inner:
  %i = phi i8 [ %n, %outer ], [ %i.next, %inner ]
  %p = load volatile ptr, ptr @port, align 2
  store volatile i8 0, ptr %p, align 1
  %i.next = add i8 %i, -1
  %i.cont = icmp ne i8 %i.next, 0
  br i1 %i.cont, label %inner, label %outer.latch

outer.latch:
  %o.next = add i8 %o, -1
  %o.cont = icmp ne i8 %o.next, 0
  br i1 %o.cont, label %outer, label %exit

exit:
  ret void
}

; In sequential loops, both loops should independently use DJNZ.
; CHECK-LABEL: _two_sequential_loops:
; CHECK:      	djnz	[[LOOP1:\.LBB[0-9_]+]]
; CHECK:      	djnz	[[LOOP2:\.LBB[0-9_]+]]
; CHECK:      	ret
define void @two_sequential_loops(i8 zeroext %n, i8 zeroext %m) {
entry:
  br label %loop1
loop1:
  %i = phi i8 [ %n, %entry ], [ %i.next, %loop1 ]
  %p1 = load volatile ptr, ptr @port, align 2
  store volatile i8 0, ptr %p1, align 1
  %i.next = add i8 %i, -1
  %c1 = icmp ne i8 %i.next, 0
  br i1 %c1, label %loop1, label %between
between:
  br label %loop2
loop2:
  %j = phi i8 [ %m, %between ], [ %j.next, %loop2 ]
  %p2 = load volatile ptr, ptr @port, align 2
  store volatile i8 1, ptr %p2, align 1
  %j.next = add i8 %j, -1
  %c2 = icmp ne i8 %j.next, 0
  br i1 %c2, label %loop2, label %exit
exit:
  ret void
}

; Under optsize / minsize, live-range splitting is disabled so code size is minimized
; (avoiding the extra preheader copy, and emitting djnz on the outer loop).
; CHECK-LABEL: _nested_djnz_optsize:
; CHECK:      	ld	b,{{ *}}a
; CHECK:      	dec	c
; CHECK-NEXT: 	jr	nz,
; CHECK:      	djnz
; CHECK:      	ret
define void @nested_djnz_optsize(i8 zeroext %m, i8 zeroext %n) optsize {
entry:
  br label %outer

outer:
  %o = phi i8 [ %m, %entry ], [ %o.next, %outer.latch ]
  br label %inner

inner:
  %i = phi i8 [ %n, %outer ], [ %i.next, %inner ]
  %p = load volatile ptr, ptr @port, align 2
  store volatile i8 0, ptr %p, align 1
  %i.next = add i8 %i, -1
  %i.cont = icmp ne i8 %i.next, 0
  br i1 %i.cont, label %inner, label %outer.latch

outer.latch:
  %o.next = add i8 %o, -1
  %o.cont = icmp ne i8 %o.next, 0
  br i1 %o.cont, label %outer, label %exit

exit:
  ret void
}
