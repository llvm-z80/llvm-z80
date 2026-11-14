; RUN: llc -mtriple=z80 -verify-machineinstrs %s -o - | FileCheck %s
; RUN: llc -mtriple=sm83 -verify-machineinstrs %s -o - | FileCheck %s --check-prefix=SM83
; RUN: llc -mtriple=z80 -stop-after=instruction-select -verify-machineinstrs %s -o - | FileCheck %s --check-prefix=ISEL
; RUN: llc -mtriple=z80 -verify-machineinstrs %s -o - | FileCheck %s --check-prefix=NEG

target triple = "z80"

@global_byte = external global i8

define i8 @or_from_ptr(ptr %p, i8 %x) {
entry:
  %mem = load i8, ptr %p, align 1
  %result = or i8 %mem, %x
  ret i8 %result
}

; CHECK-LABEL: _or_from_ptr:
; CHECK:       or (hl)
; SM83-LABEL:  _or_from_ptr:
; SM83:        or (hl)
; ISEL-LABEL: name: or_from_ptr
; ISEL:        OR8_IND
; ISEL-SAME:   load (s8) from %ir.p

define i8 @or_from_global(i8 %x) {
entry:
  %mem = load i8, ptr @global_byte, align 1
  %result = or i8 %mem, %x
  ret i8 %result
}

; CHECK-LABEL: _or_from_global:
; CHECK:       ld hl,_global_byte
; CHECK-NEXT:  or (hl)
; ISEL-LABEL: name: or_from_global
; ISEL:        OR8_IND
; ISEL-SAME:   load (s8) from @global_byte

define i8 @or_load_second(ptr %p, i8 %x) {
entry:
  %mem = load i8, ptr %p, align 1
  %result = or i8 %x, %mem
  ret i8 %result
}

; CHECK-LABEL: _or_load_second:
; CHECK:       or (hl)

define i8 @or_volatile_load(ptr %p, i8 %x) {
entry:
  %mem = load volatile i8, ptr %p, align 1
  %result = or i8 %mem, %x
  ret i8 %result
}

; CHECK-LABEL: _or_volatile_load:
; CHECK:       or (hl)
; ISEL-LABEL: name: or_volatile_load
; ISEL:        OR8_IND
; ISEL-SAME:   volatile load (s8) from %ir.p

define i8 @or_multi_use_load(ptr %p, i8 %x) {
entry:
  %mem = load i8, ptr %p, align 1
  %or = or i8 %mem, %x
  %xor = xor i8 %mem, 1
  %result = add i8 %or, %xor
  ret i8 %result
}

; CHECK-LABEL: _or_multi_use_load:
; CHECK-NOT:   or (hl)
; CHECK:       ld a,(hl)
; NEG-LABEL:   _or_multi_use_load:
; NEG-NOT:     or (hl)
; NEG:         ld a,(hl)

define i8 @or_after_aliasing_store(ptr %p, i8 %x) {
entry:
  %mem = load i8, ptr %p, align 1
  store i8 42, ptr %p, align 1
  %result = or i8 %mem, %x
  ret i8 %result
}

; CHECK-LABEL: _or_after_aliasing_store:
; CHECK-NOT:   or (hl)
; CHECK:       ld c,(hl)
; CHECK:       ld (hl),a
; CHECK:       or b
; NEG-LABEL:   _or_after_aliasing_store:
; NEG-NOT:     or (hl)
; NEG:         ld c,(hl)
; NEG:         ld (hl),a
; NEG:         or b
