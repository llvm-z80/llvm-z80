; RUN: llc -verify-machineinstrs -mtriple=z80 -O0 < %s | FileCheck %s

; A frame slot load stays ahead of a later store to the same slot.

; CHECK-LABEL: fold8:
; CHECK:       ld (ix-[[S:[0-9]+]]),a
; CHECK-NEXT:  ld {{[a-z]}},(ix-[[S]])
; CHECK-NEXT:  ld (ix-[[S]]),l
; CHECK-NOT:   (ix-[[S]])
; CHECK:       ret
define i8 @fold8(i8 %x, i8 %y) "frame-pointer"="all" {
  %a = alloca i8
  store volatile i8 %x, ptr %a
  %v = load i8, ptr %a
  store i8 %y, ptr %a
  %r = add i8 %y, %v
  ret i8 %r
}

; CHECK-LABEL: fold16:
; CHECK:       ld (ix-[[S:[0-9]+]]),l
; CHECK:       ld {{[a-z]}},(ix-[[S]])
; CHECK:       ld (ix-[[S]]),e
; CHECK-NOT:   ,(ix-[[S]])
; CHECK:       ret
define i16 @fold16(i16 %x, i16 %y) "frame-pointer"="all" {
  %a = alloca [4 x i16]
  %a1 = getelementptr i16, ptr %a, i16 1
  %a2 = getelementptr i16, ptr %a, i16 2
  store i16 %x, ptr %a
  store i16 %x, ptr %a1
  store i16 %x, ptr %a2
  %v0 = load i16, ptr %a
  %v1 = load i16, ptr %a1
  %v2 = load i16, ptr %a2
  store i16 %y, ptr %a
  %s0 = add i16 %y, %v0
  %s1 = add i16 %s0, %v1
  %s2 = add i16 %s1, %v2
  ret i16 %s2
}
