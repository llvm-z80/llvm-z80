; RUN: llc -mtriple=z80 -verify-machineinstrs %s -o - | FileCheck %s
; RUN: llc -mtriple=z80 -stop-after=instruction-select -verify-machineinstrs %s -o - | FileCheck %s --check-prefix=ISEL
; RUN: llc -mtriple=sm83 -verify-machineinstrs %s -o - | FileCheck %s --check-prefix=SM83

; INC (HL) / DEC (HL) fold: load/arith-by-1/store on the same pointer collapses
; to the 1-byte memory-direct form 0x34 / 0x35.

target triple = "z80"

@counter = external global i8

define void @inc_ptr(ptr %p) {
entry:
  %v = load i8, ptr %p, align 1
  %r = add i8 %v, 1
  store i8 %r, ptr %p, align 1
  ret void
}

; CHECK-LABEL: _inc_ptr:
; CHECK:       inc (hl)
; CHECK-NOT:   ld a,
; ISEL-LABEL: name: inc_ptr
; ISEL:        INC8_IND
; SM83-LABEL:  _inc_ptr:
; SM83:        inc (hl)

define void @dec_ptr(ptr %p) {
entry:
  %v = load i8, ptr %p, align 1
  %r = sub i8 %v, 1
  store i8 %r, ptr %p, align 1
  ret void
}

; CHECK-LABEL: _dec_ptr:
; CHECK:       dec (hl)
; CHECK-NOT:   ld a,
; ISEL-LABEL: name: dec_ptr
; ISEL:        DEC8_IND

define void @add_neg1(ptr %p) {
entry:
  %v = load i8, ptr %p, align 1
  %r = add i8 %v, -1
  store i8 %r, ptr %p, align 1
  ret void
}

; CHECK-LABEL: _add_neg1:
; CHECK:       dec (hl)
; ISEL-LABEL: name: add_neg1
; ISEL:        DEC8_IND

define void @inc_global() {
entry:
  %v = load i8, ptr @counter, align 1
  %r = add i8 %v, 1
  store i8 %r, ptr @counter, align 1
  ret void
}

; CHECK-LABEL: _inc_global:
; CHECK:       ld hl,_counter
; CHECK-NEXT:  inc (hl)
; CHECK-NOT:   ld a,(_counter)
; ISEL-LABEL: name: inc_global
; ISEL:        INC8_IND

define void @dec_global() {
entry:
  %v = load i8, ptr @counter, align 1
  %r = sub i8 %v, 1
  store i8 %r, ptr @counter, align 1
  ret void
}

; CHECK-LABEL: _dec_global:
; CHECK:       ld hl,_counter
; CHECK-NEXT:  dec (hl)

; Commutative form: add i8 1, %v
define void @inc_commuted(ptr %p) {
entry:
  %v = load i8, ptr %p, align 1
  %r = add i8 1, %v
  store i8 %r, ptr %p, align 1
  ret void
}

; CHECK-LABEL: _inc_commuted:
; CHECK:       inc (hl)

; Volatile load must NOT fold (would collapse two accesses into one R-M-W).
define void @volatile_load(ptr %p) {
entry:
  %v = load volatile i8, ptr %p, align 1
  %r = add i8 %v, 1
  store i8 %r, ptr %p, align 1
  ret void
}

; CHECK-LABEL: _volatile_load:
; CHECK-NOT:   inc (hl)
; CHECK:       ld a,(hl)

; Volatile store must NOT fold either.
define void @volatile_store(ptr %p) {
entry:
  %v = load i8, ptr %p, align 1
  %r = add i8 %v, 1
  store volatile i8 %r, ptr %p, align 1
  ret void
}

; CHECK-LABEL: _volatile_store:
; CHECK-NOT:   inc (hl)
; CHECK:       ld a,

; A second use of the loaded byte must inhibit the fold.
define i8 @multi_use_load(ptr %p) {
entry:
  %v = load i8, ptr %p, align 1
  %r = add i8 %v, 1
  store i8 %r, ptr %p, align 1
  ret i8 %v
}

; CHECK-LABEL: _multi_use_load:
; CHECK-NOT:   inc (hl)

; An intervening memory op must inhibit the fold.
define void @aliasing_store(ptr %p, ptr %q) {
entry:
  %v = load i8, ptr %p, align 1
  store i8 42, ptr %q, align 1
  %r = add i8 %v, 1
  store i8 %r, ptr %p, align 1
  ret void
}

; CHECK-LABEL: _aliasing_store:
; CHECK-NOT:   inc (hl)

; +-2 must NOT fold (only +-1 is allowed).
define void @add_two(ptr %p) {
entry:
  %v = load i8, ptr %p, align 1
  %r = add i8 %v, 2
  store i8 %r, ptr %p, align 1
  ret void
}

; CHECK-LABEL: _add_two:
; CHECK-NOT:   inc (hl)
; CHECK:       ld a,(hl)
