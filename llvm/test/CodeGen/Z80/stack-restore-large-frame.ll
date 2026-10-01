; RUN: llc -mtriple=z80 -O2 -verify-machineinstrs < %s | FileCheck %s
;
; A saved stack pointer spilled to a slot out of reach of (IX+d) is reloaded
; into a pair and moved to SP from there. The copy to SP is not folded into
; the reload, which has no form that writes SP.

define ptr @restore(ptr %sp, i16 %i) "frame-pointer"="all" {
; CHECK-LABEL: _restore:
; CHECK:       ; %restore
; CHECK:       ld b,(hl)
; CHECK-NEXT:  ld l,c
; CHECK-NEXT:  ld h,b
; CHECK-NEXT:  ld sp,hl
entry:
  %data = alloca [64 x i16], align 1
  store i16 0, ptr %data, align 1
  %p = getelementptr [2 x i8], ptr %data, i16 %i
  %v = load i16, ptr %p, align 1
  %z = icmp eq i16 %v, 0
  br i1 %z, label %restore, label %call

call:
  tail call void @abort()
  ret ptr null

restore:
  call void @llvm.stackrestore.p0(ptr %sp)
  call void @llvm.lifetime.end.p0(ptr %data)
  ret ptr null
}

declare void @abort()
