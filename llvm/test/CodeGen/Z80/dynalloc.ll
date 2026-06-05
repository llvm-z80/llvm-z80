; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O0 < %s | FileCheck %s

; Test G_DYN_STACKALLOC (alloca with runtime size)

define i16 @dynamic_alloca(i16 %n) {
; CHECK-LABEL: dynamic_alloca:
; CHECK: add hl,sp
; CHECK: sbc hl,
; CHECK: ld sp,hl
; CHECK: ret
  %buf = alloca i8, i16 %n, align 1
  store i8 42, ptr %buf
  %v = load i8, ptr %buf
  %r = zext i8 %v to i16
  ret i16 %r
}
