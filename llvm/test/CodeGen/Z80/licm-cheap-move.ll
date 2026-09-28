; RUN: llc -mtriple=z80 -O2 -stop-after=early-machinelicm < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,Z80
; RUN: llc -mtriple=sm83 -O2 -stop-after=early-machinelicm < %s | FileCheck %s

; An instruction as cheap as a move stays in the loop; a costlier invariant
; is still hoisted.

; CHECK-LABEL: name: stride
; CHECK:       bb.1.entry:
; CHECK-NOT:   LD_rr_nn 1000
; CHECK:       bb.2.loop:
; CHECK:       LD_rr_nn 1000
define i16 @stride(ptr %p, i16 %n) {
entry:
  br label %loop
loop:
  %i = phi i16 [ 0, %entry ], [ %i.next, %loop ]
  %q = phi ptr [ %p, %entry ], [ %q.next, %loop ]
  %s = phi i16 [ 0, %entry ], [ %s.next, %loop ]
  %v = load i16, ptr %q
  %s.next = add i16 %s, %v
  %q.next = getelementptr i8, ptr %q, i16 1000
  %i.next = add i16 %i, 1
  %c = icmp ne i16 %i.next, %n
  br i1 %c, label %loop, label %exit
exit:
  ret i16 %s.next
}

@g = external global i16

; SM83 has no absolute 16-bit load to hoist.
; Z80-LABEL: name: invariant_load
; Z80:       bb.1.entry:
; Z80:       LOAD16_ABS @g
; Z80:       bb.2.loop:
; Z80-NOT:   LOAD16_ABS
; Z80:       bb.3.exit:
define i16 @invariant_load(ptr %p, i16 %n) {
entry:
  br label %loop
loop:
  %i = phi i16 [ 0, %entry ], [ %i.next, %loop ]
  %q = phi ptr [ %p, %entry ], [ %q.next, %loop ]
  %s = phi i16 [ 0, %entry ], [ %s.next, %loop ]
  %v = load i16, ptr %q
  %k = load i16, ptr @g, !invariant.load !0
  %t = add i16 %v, %k
  %s.next = add i16 %s, %t
  %q.next = getelementptr i8, ptr %q, i16 2
  %i.next = add i16 %i, 1
  %c = icmp ne i16 %i.next, %n
  br i1 %c, label %loop, label %exit
exit:
  ret i16 %s.next
}

!0 = !{}
