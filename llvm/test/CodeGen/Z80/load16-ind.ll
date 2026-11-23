; RUN: llc -mtriple=z80 -O2 -verify-machineinstrs < %s | FileCheck %s --check-prefix=Z80
; RUN: llc -mtriple=sm83 -O2 -verify-machineinstrs < %s | FileCheck %s --check-prefix=SM83

; A 16-bit load or store through a pointer uses any pair for the value. HL
; steps from one access to the next, and a load may land in HL itself.

define i16 @two_fields(ptr %p) {
; Z80-LABEL:  _two_fields:
; Z80:        ld c,(hl)
; Z80-NEXT:   inc hl
; Z80-NEXT:   ld b,(hl)
; Z80-NEXT:   inc hl
; Z80-NEXT:   ld e,(hl)
; Z80-NEXT:   inc hl
; Z80-NEXT:   ld d,(hl)
; SM83-LABEL: _two_fields:
; SM83:       ld a,(hl+)
; SM83-NEXT:  ld c,a
; SM83-NEXT:  ld a,(hl+)
; SM83-NEXT:  ld b,a
; SM83-NEXT:  ld a,(hl+)
; SM83-NEXT:  ld e,a
; SM83-NEXT:  ld d,(hl)
  %x = load i16, ptr %p
  %yp = getelementptr inbounds i8, ptr %p, i16 2
  %y = load i16, ptr %yp
  %s = add i16 %x, %y
  ret i16 %s
}

define i16 @load_through_loaded(ptr %pp) {
; Z80-LABEL:  _load_through_loaded:
; Z80:        ld a,(hl)
; Z80-NEXT:   inc hl
; Z80-NEXT:   ld h,(hl)
; Z80-NEXT:   ld l,a
; Z80-NEXT:   ld e,(hl)
; Z80-NEXT:   inc hl
; Z80-NEXT:   ld d,(hl)
; Z80-NEXT:   ret
; SM83-LABEL: _load_through_loaded:
; SM83:       ld a,(hl+)
; SM83-NEXT:  ld h,(hl)
; SM83-NEXT:  ld l,a
; SM83-NEXT:  ld a,(hl+)
; SM83-NEXT:  ld c,a
; SM83-NEXT:  ld b,(hl)
; SM83-NEXT:  ret
  %p = load ptr, ptr %pp
  %v = load i16, ptr %p
  ret i16 %v
}

define void @store_two(ptr %p, i16 %v) {
; Z80-LABEL:  _store_two:
; Z80:        ld (hl),e
; Z80-NEXT:   inc hl
; Z80-NEXT:   ld (hl),d
; Z80-NEXT:   inc hl
; Z80-NEXT:   ld (hl),e
; Z80-NEXT:   inc hl
; Z80-NEXT:   ld (hl),d
; Z80-NEXT:   ret
; SM83-LABEL: _store_two:
; SM83:       ld a,c
; SM83-NEXT:  ld (hl+),a
; SM83-NEXT:  ld a,b
; SM83-NEXT:  ld (hl+),a
; SM83-NEXT:  ld a,c
; SM83-NEXT:  ld (hl+),a
; SM83-NEXT:  ld (hl),b
; SM83-NEXT:  ret
  store i16 %v, ptr %p
  %q = getelementptr inbounds i8, ptr %p, i16 2
  store i16 %v, ptr %q
  ret void
}
