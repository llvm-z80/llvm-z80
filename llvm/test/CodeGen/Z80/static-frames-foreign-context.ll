; RUN: llc -mtriple=z80 -O1 < %s | FileCheck %s
; RUN: llc -mtriple=sm83 -O1 < %s | FileCheck %s

; A function that cleared the static-frame feature (written in C as
; __attribute__((target("no-static-frame")))) is entered from an execution
; context the module analysis cannot see, such as an interrupt handler in
; foreign code. It keeps its stack frame, and so does everything it can
; reach, since any of that may run concurrently with any other context.
; Functions outside that tree still get their static frames.

; CHECK-NOT: from_asm_isr.frame
; CHECK-NOT: isr_helper.frame
; CHECK: normal.frame
define void @from_asm_isr(i16 %x) #0 {
  %s = alloca [4 x i16]
  %p = getelementptr [4 x i16], ptr %s, i16 0, i16 1
  store volatile i16 %x, ptr %p
  call void @isr_helper(i16 %x)
  ret void
}

define internal void @isr_helper(i16 %x) {
  %s = alloca [4 x i16]
  %p = getelementptr [4 x i16], ptr %s, i16 0, i16 1
  store volatile i16 %x, ptr %p
  ret void
}

define void @normal(i16 %x) {
  %s = alloca [4 x i16]
  %p = getelementptr [4 x i16], ptr %s, i16 0, i16 1
  store volatile i16 %x, ptr %p
  ret void
}

; The excluded tree's block copy becomes a memcpy call only during
; instruction selection, so the module's own memcpy is part of the tree
; even though no IR edge says so.
; CHECK-NOT: memcpy.frame
define ptr @memcpy(ptr %d, ptr %s, i16 %n) norecurse {
entry:
  %saved = alloca ptr
  store volatile ptr %d, ptr %saved
  br label %loop
loop:
  %i = phi i16 [ 0, %entry ], [ %inc, %loop ]
  %sp = getelementptr i8, ptr %s, i16 %i
  %dp = getelementptr i8, ptr %d, i16 %i
  %b = load i8, ptr %sp
  store i8 %b, ptr %dp
  %inc = add i16 %i, 1
  %c = icmp ult i16 %inc, %n
  br i1 %c, label %loop, label %done
done:
  %r = load volatile ptr, ptr %saved
  ret ptr %r
}

@src = global [32 x i8] zeroinitializer
@dst = global [32 x i8] zeroinitializer

define void @copies(i16 %x) #0 {
  call void @llvm.memcpy.p0.p0.i16(ptr @dst, ptr @src, i16 32, i1 false)
  ret void
}

declare void @llvm.memcpy.p0.p0.i16(ptr, ptr, i16, i1)

attributes #0 = { "target-features"="-static-frame" }

; The frame aliases are defined after the last function, so check the
; excluded tree again past the positive match above.
; CHECK-NOT: from_asm_isr.frame
; CHECK-NOT: isr_helper.frame
; CHECK-NOT: memcpy.frame
