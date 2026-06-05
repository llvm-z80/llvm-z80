; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s

; Test G_PTRTOINT and G_INTTOPTR (no-op on Z80, both 16-bit)

define i16 @ptrtoint(ptr %p) {
; CHECK-LABEL: _ptrtoint:
; CHECK: ex de,hl
; CHECK-NEXT: ret
  %v = ptrtoint ptr %p to i16
  ret i16 %v
}

define ptr @inttoptr(i16 %v) {
; CHECK-LABEL: _inttoptr:
; CHECK: ex de,hl
; CHECK-NEXT: ret
  %p = inttoptr i16 %v to ptr
  ret ptr %p
}

; Pointer subtraction with volatile to prevent folding
@g = global i16 0

define i16 @ptr_sub(ptr %a, ptr %b) {
; CHECK-LABEL: _ptr_sub:
; CHECK: sbc hl,
; CHECK: ret
  %ai = ptrtoint ptr %a to i16
  %bi = ptrtoint ptr %b to i16
  %diff = sub i16 %ai, %bi
  ret i16 %diff
}
