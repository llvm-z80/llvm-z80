; In the z88dk environment CallingConv::C is __sdcccall(0), and cc 240 keeps
; __sdcccall(1).
; RUN: llc -mtriple=z80-unknown-none-z88dk -O1 < %s | FileCheck %s

declare i16 @ext(i16, i16)
declare cc 240 i16 @ext1(i16, i16)

; CHECK-LABEL: _call_ext:
; CHECK:      ld hl,2
; CHECK-NEXT: push hl
; CHECK-NEXT: ld hl,1
; CHECK-NEXT: push hl
; CHECK-NEXT: call _ext
; CHECK-NEXT: pop af
; CHECK-NEXT: pop af
; CHECK-NEXT: ret
define i16 @call_ext() {
  %r = call i16 @ext(i16 1, i16 2)
  ret i16 %r
}

; CHECK-LABEL: _add1:
; CHECK:      add hl,de
; CHECK-NEXT: ex de,hl
; CHECK-NEXT: ret
define cc 240 i16 @add1(i16 %a, i16 %b) {
  %s = add i16 %a, %b
  ret i16 %s
}

; The __sdcccall(1) result comes back in DE and leaves in HL.
; CHECK-LABEL: _call_ext1:
; CHECK:      ld hl,1
; CHECK-NEXT: ld de,2
; CHECK-NEXT: call _ext1
; CHECK-NEXT: ex de,hl
; CHECK-NEXT: ret
define i16 @call_ext1() {
  %r = call cc 240 i16 @ext1(i16 1, i16 2)
  ret i16 %r
}
