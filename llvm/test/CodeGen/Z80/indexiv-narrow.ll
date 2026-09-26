; RUN: opt -mtriple=z80 -passes='loop(z80-indexiv)' -S < %s | FileCheck %s
; RUN: opt -mtriple=sm83 -passes='loop(z80-indexiv)' -S < %s | FileCheck %s

; A loop variable that only takes values that fit in a byte, and whose every
; use wants no more than its low byte, counts in 8 bits; the wide variable
; goes away.

define void @copy16(ptr %d, ptr %s) {
; CHECK-LABEL: define void @copy16(
; CHECK-NOT:     phi i16
; CHECK:         [[DONE:%.*]] = icmp eq i8 {{%.*}}, 16
; CHECK:         br i1 [[DONE]]
entry:
  br label %loop
loop:
  %i = phi i16 [ 0, %entry ], [ %i.next, %loop ]
  %sp = getelementptr i8, ptr %s, i16 %i
  %v = load i8, ptr %sp
  %dp = getelementptr i8, ptr %d, i16 %i
  store i8 %v, ptr %dp
  %i.next = add nuw nsw i16 %i, 1
  %done = icmp eq i16 %i.next, 16
  br i1 %done, label %exit, label %loop
exit:
  ret void
}

; 300 iterations do not fit a byte.
define void @copy300(ptr %d, ptr %s) {
; CHECK-LABEL: define void @copy300(
; CHECK:         icmp eq i16 {{%.*}}, 300
entry:
  br label %loop
loop:
  %i = phi i16 [ 0, %entry ], [ %i.next, %loop ]
  %sp = getelementptr i8, ptr %s, i16 %i
  %v = load i8, ptr %sp
  %dp = getelementptr i8, ptr %d, i16 %i
  store i8 %v, ptr %dp
  %i.next = add nuw nsw i16 %i, 1
  %done = icmp eq i16 %i.next, 300
  br i1 %done, label %exit, label %loop
exit:
  ret void
}

; A byte of it is stored and it indexes the store; the index is 8-bit
; already, and the byte is the counter itself.
define void @count_byte(ptr %d) {
; CHECK-LABEL: define void @count_byte(
; CHECK-NOT:     phi i16
; CHECK:         [[IV:%.*]] = phi i8
; CHECK:         store i8 [[IV]]
; CHECK:         icmp eq i8 {{%.*}}, 16
entry:
  br label %loop
loop:
  %i = phi i16 [ 0, %entry ], [ %i.next, %loop ]
  %dp = getelementptr i8, ptr %d, i16 %i
  %b = trunc i16 %i to i8
  store i8 %b, ptr %dp
  %i.next = add nuw nsw i16 %i, 1
  %done = icmp eq i16 %i.next, 16
  br i1 %done, label %exit, label %loop
exit:
  ret void
}

; A signed order against a bound in 0-255 is the unsigned one in a byte.
define void @signed_bound(ptr %d, i8 %n) {
; CHECK-LABEL: define void @signed_bound(
; CHECK-NOT:     phi i16
; CHECK:         icmp ult i8
entry:
  %bound = zext i8 %n to i16
  %any = icmp eq i8 %n, 0
  br i1 %any, label %exit, label %loop
loop:
  %i = phi i16 [ 0, %entry ], [ %i.next, %loop ]
  store volatile i8 0, ptr %d
  %i.next = add nuw nsw i16 %i, 1
  %more = icmp slt i16 %i.next, %bound
  br i1 %more, label %loop, label %exit
exit:
  ret void
}

; The whole wide value is wanted for something else, so it stays.
define void @count_stored(ptr %d) {
; CHECK-LABEL: define void @count_stored(
; CHECK:         icmp eq i16 {{%.*}}, 16
entry:
  br label %loop
loop:
  %i = phi i16 [ 0, %entry ], [ %i.next, %loop ]
  store volatile i16 %i, ptr %d
  %i.next = add nuw nsw i16 %i, 1
  %done = icmp eq i16 %i.next, 16
  br i1 %done, label %exit, label %loop
exit:
  ret void
}
