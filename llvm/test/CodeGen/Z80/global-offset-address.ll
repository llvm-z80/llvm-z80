; RUN: llc -verify-machineinstrs -mtriple=z80 -O1 < %s | FileCheck %s --check-prefixes=CHECK,Z80
; RUN: llc -verify-machineinstrs -mtriple=sm83 -O1 < %s | FileCheck %s --check-prefixes=CHECK,SM83

; A global displaced by a constant is one immediate for the linker to settle,
; rather than the global and the displacement added at run time.

@w = global [10 x i8] zeroinitializer

define void @store_at_offset(i8 %v) {
; CHECK-LABEL: store_at_offset:
; Z80:           ld (_w+5),a
; SM83:          ld bc,_w+5
; SM83-NEXT:     ld (bc),a
; CHECK-NEXT:    ret
  store i8 %v, ptr getelementptr (i8, ptr @w, i16 5)
  ret void
}

define i8 @load_at_offset() {
; CHECK-LABEL: load_at_offset:
; Z80:           ld a,(_w+3)
; SM83:          ld bc,_w+3
; SM83-NEXT:     ld a,(bc)
; CHECK-NEXT:    ret
  %v = load i8, ptr getelementptr (i8, ptr @w, i16 3)
  ret i8 %v
}

define ptr @address_at_offset() {
; CHECK-LABEL: address_at_offset:
; Z80:           ld de,_w+5
; SM83:          ld bc,_w+5
; CHECK-NEXT:    ret
  ret ptr getelementptr (i8, ptr @w, i16 5)
}

define void @store_before(i8 %v) {
; CHECK-LABEL: store_before:
; Z80:           ld (_w-1),a
; SM83:          ld bc,_w-1
; SM83-NEXT:     ld (bc),a
  store i8 %v, ptr getelementptr (i8, ptr @w, i16 -1)
  ret void
}

; Each address is its own constant even next to another one from the same
; global, materialized where it is used rather than stepped from a register
; that would have to stay live.
define void @nearby(i8 %v) {
; CHECK-LABEL: nearby:
; Z80:           ld (_w+1),a
; Z80-NEXT:      ld (_w+2),a
; SM83:          ld bc,_w+1
; SM83-NEXT:     ld (bc),a
; SM83-NEXT:     ld bc,_w+2
; SM83-NEXT:     ld (bc),a
; CHECK-NEXT:    ret
  store i8 %v, ptr getelementptr (i8, ptr @w, i16 1)
  store i8 %v, ptr getelementptr (i8, ptr @w, i16 2)
  ret void
}

; Splitting a wide access adds displacements of its own after the IR is gone;
; those fold the same way.
@l = global i32 0

define i32 @split_load() {
; CHECK-LABEL: split_load:
; Z80:           ld {{de|hl|bc}},(_l+2)
; SM83:          ld hl,_l+2
  %v = load i32, ptr @l
  ret i32 %v
}
