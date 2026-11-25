; RUN: llc -mtriple=z80 -O2 -verify-machineinstrs < %s | FileCheck %s --check-prefixes=CHECK,Z80
; RUN: llc -mtriple=sm83 -O2 -verify-machineinstrs < %s | FileCheck %s --check-prefixes=CHECK,SM83
;
; A caller keeps values in the listed registers across the call. A body
; with the attribute saves each listed register it changes, except a result
; register, and does not tail call.

declare void @bc() "z80-preserves-regs"="b,c"
declare void @plain()

; CHECK-LABEL: _keep:
; Z80:         ld c,a
; Z80-NEXT:    ld b,l
; SM83:        ld c,a
; SM83-NEXT:   ld b,e
; CHECK-NEXT:  call _bc
; CHECK-NEXT:  ld a,c
; CHECK-NEXT:  add a,b
; CHECK-NEXT:  ret
define i8 @keep(i8 %x, i8 %y) {
  call void @bc()
  %s = add i8 %x, %y
  ret i8 %s
}

; CHECK-LABEL: _saves:
; CHECK:       push bc
; CHECK-NEXT:  push de
; CHECK-NEXT:  call _plain
; CHECK-NEXT:  pop de
; CHECK-NEXT:  pop bc
; CHECK-NEXT:  ret
define void @saves() "z80-preserves-regs"="b,c,d,e" {
  call void @plain()
  ret void
}

; A pair name keeps both halves.
; CHECK-LABEL: _pair:
; CHECK:       push bc
; CHECK-NEXT:  call _plain
; CHECK-NEXT:  pop bc
; CHECK-NEXT:  ret
define void @pair() "z80-preserves-regs"="bc" {
  call void @plain()
  ret void
}

; CHECK-LABEL: _untouched:
; CHECK-NEXT:  ; %bb.0:
; CHECK-NEXT:  ret
define void @untouched() "z80-preserves-regs"="b,c,d,e" {
  ret void
}

; A returns the result, so only BC is saved.
; CHECK-LABEL: _ret_a:
; CHECK:       push bc
; CHECK-NEXT:  call _plain
; CHECK-NEXT:  ld a,7
; CHECK-NEXT:  pop bc
; CHECK-NEXT:  ret
define i8 @ret_a() "z80-preserves-regs"="a,b" {
  call void @plain()
  ret i8 7
}

; A alone is saved with F.
; CHECK-LABEL: _a_only:
; CHECK:       push af
; CHECK-NEXT:  call _plain
; CHECK-NEXT:  pop af
; CHECK-NEXT:  ret
define void @a_only() "z80-preserves-regs"="a" {
  call void @plain()
  ret void
}

; CHECK-LABEL: _no_tail:
; CHECK:       push bc
; CHECK-NEXT:  call _plain
; CHECK-NEXT:  pop bc
; CHECK-NEXT:  ret
define void @no_tail() "z80-preserves-regs"="b" {
  tail call void @plain()
  ret void
}

; SM83 has no IX or IY.
; CHECK-LABEL: _index_body:
; Z80:         push iy
; SM83-NOT:    push
; CHECK:       call _plain
; Z80-NEXT:    pop iy
; CHECK-NEXT:  ret
define void @index_body() "z80-preserves-regs"="iyl,iyh,ixl" {
  call void @plain()
  ret void
}
