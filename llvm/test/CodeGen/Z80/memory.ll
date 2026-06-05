; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O0 < %s | FileCheck %s

@global_var = global i16 0

; Test: load from global variable
define i16 @load_global() {
; CHECK-LABEL: _load_global:
; CHECK:       ld hl,#_global_var
; CHECK:       ld e,(hl)
; CHECK-NEXT:  inc hl
; CHECK-NEXT:  ld d,(hl)
; CHECK-NEXT:  ret
  %v = load i16, ptr @global_var
  ret i16 %v
}

; Test: store to global variable
define void @store_global(i16 %v) {
; CHECK-LABEL: _store_global:
; CHECK:       ld hl,#_global_var
; CHECK:       ld (hl),e
; CHECK-NEXT:  inc hl
; CHECK-NEXT:  ld (hl),d
  store i16 %v, ptr @global_var
  ret void
}

; Test: zero-extend i8 to i16
define i16 @zext_i8_to_i16(i8 %a) {
; CHECK-LABEL: _zext_i8_to_i16:
; CHECK:       ld e,a
; CHECK-NEXT:  ld d,#0
; CHECK-NEXT:  ret
  %w = zext i8 %a to i16
  ret i16 %w
}

; Test: truncate i16 to i8
define i8 @trunc_i16_to_i8(i16 %a) {
; CHECK-LABEL: _trunc_i16_to_i8:
; CHECK:       ld a,l
; CHECK-NEXT:  ret
  %t = trunc i16 %a to i8
  ret i8 %t
}

; Test: sign-extend i8 to i16
define i16 @sext_i8_to_i16(i8 %a) {
; CHECK-LABEL: _sext_i8_to_i16:
; CHECK:       ld e,a
; CHECK-NEXT:  rlca
; CHECK-NEXT:  sbc a,a
; CHECK-NEXT:  ld d,a
; CHECK-NEXT:  ret
  %w = sext i8 %a to i16
  ret i16 %w
}

; Test: 16-bit multiply uses library call
define i16 @mul16(i16 %a, i16 %b) {
; CHECK-LABEL: _mul16:
; CHECK:       call ___mulhi3
  %c = mul i16 %a, %b
  ret i16 %c
}
