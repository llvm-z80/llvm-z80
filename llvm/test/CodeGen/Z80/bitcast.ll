; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s

; Test G_BITCAST between pointer and integer types (no-op on Z80)

define i16 @bitcast_ptr_to_int(ptr %p) {
; CHECK-LABEL: _bitcast_ptr_to_int:
; CHECK: ex de,hl
; CHECK-NEXT: ret
  %v = ptrtoint ptr %p to i16
  ret i16 %v
}

define ptr @bitcast_int_to_ptr(i16 %v) {
; CHECK-LABEL: _bitcast_int_to_ptr:
; CHECK: ex de,hl
; CHECK-NEXT: ret
  %p = inttoptr i16 %v to ptr
  ret ptr %p
}
