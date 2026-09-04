; RUN: llc -verify-machineinstrs -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s --check-prefix=Z80
; RUN: llc -verify-machineinstrs -mtriple=sm83 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s --check-prefix=SM83
;
; A volatile local is an access the program asked to perform. The memory
; operand that says so rides the frame index pseudo from selection through
; expansion, so the peepholes that forward stack values can tell one from a
; spill slot and leave it alone.

declare void @use(i16, i16)

define void @volatile_reload(i16 %x) "frame-pointer"="all" {
  %p = alloca i16
  store volatile i16 %x, ptr %p
  %a = load volatile i16, ptr %p
  %b = load volatile i16, ptr %p
  call void @use(i16 %a, i16 %b)
  ret void
}

; Both reads reach the slot; neither is answered from the register the store
; came out of.

; Z80-LABEL: _volatile_reload:
; Z80:      ld -2(ix),l
; Z80:      ld l,-2(ix)
; Z80-NEXT: ld h,-1(ix)
; Z80:      ld e,-2(ix)
; Z80-NEXT: ld d,-1(ix)

; SM83-LABEL: _volatile_reload:
; SM83:      ld a,(hl+)
; SM83:      ld a,(hl+)
