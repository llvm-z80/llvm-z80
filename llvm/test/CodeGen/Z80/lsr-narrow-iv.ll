; RUN: llc -mtriple=z80 -O2 -verify-machineinstrs %s -o - | FileCheck %s

; Verifies that LSR prefers a narrow 8-bit down-counter IV.
target datalayout = "e-m:e-p:16:8-i8:8-i16:8-i24:8-i32:8-a:8-n8:16-S8"
target triple = "z80"

define void @countdown(ptr %p, i8 %count) {
entry:
  %tobool.not1 = icmp eq i8 %count, 0
  br i1 %tobool.not1, label %while.end, label %while.body

while.body:
  %p.addr.03 = phi ptr [ %incdec.ptr, %while.body ], [ %p, %entry ]
  %count.addr.02 = phi i8 [ %dec, %while.body ], [ %count, %entry ]
  %dec = add i8 %count.addr.02, -1
  %incdec.ptr = getelementptr inbounds i8, ptr %p.addr.03, i16 1
  store i8 0, ptr %p.addr.03, align 1
  %tobool.not = icmp eq i8 %dec, 0
  br i1 %tobool.not, label %while.end, label %while.body

while.end:
  ret void
}

; CHECK-LABEL: _countdown:
; CHECK:       dec b
; CHECK:       inc hl
; CHECK:       jr nz,
