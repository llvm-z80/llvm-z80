; RUN: llc -verify-machineinstrs -mtriple=z80 -O1 < %s | FileCheck %s --check-prefix=Z80
; RUN: llc -verify-machineinstrs -mtriple=z80 -O0 < %s | FileCheck %s --check-prefix=Z80
; RUN: llc -verify-machineinstrs -mtriple=sm83 -O1 < %s | FileCheck %s --check-prefix=SM83

; A 16-bit access to an address the linker settles takes one instruction here,
; either LD HL,(nn) / LD (nn),HL or the ED-prefixed BC and DE forms. The address
; never reaches a pointer register, so the two byte accesses through it and the
; address materialization they need both go away.
;
; SM83 has no such instruction, so it keeps reading the bytes through a pointer.

@counter = global i16 0
@arr = global [4 x i16] zeroinitializer
@ptr = global ptr null
@big = global [8 x i8] zeroinitializer

define i16 @load_global() {
; Z80-LABEL: load_global:
; Z80:         ld de,(_counter)
; Z80-NEXT:    ret
;
; SM83-LABEL: load_global:
; SM83:        ld hl,_counter
  %v = load i16, ptr @counter
  ret i16 %v
}

define void @store_global(i16 %v) {
; Z80-LABEL: store_global:
; Z80:         ld (_counter),hl
; Z80-NEXT:    ret
;
; SM83-LABEL: store_global:
; SM83:        ld hl,_counter
  store i16 %v, ptr @counter
  ret void
}

; A constant displacement rides along in the symbol operand rather than being
; added into a register.
define i16 @load_element() {
; Z80-LABEL: load_element:
; Z80:         ld de,(_arr+4)
; Z80-NEXT:    ret
  %p = getelementptr [4 x i16], ptr @arr, i16 0, i16 2
  %v = load i16, ptr %p
  ret i16 %v
}

define void @store_element(i16 %v) {
; Z80-LABEL: store_element:
; Z80:         ld (_arr+6),hl
; Z80-NEXT:    ret
  %p = getelementptr [4 x i16], ptr @arr, i16 0, i16 3
  store i16 %v, ptr %p
  ret void
}

; A pointer-sized value is the same access.
define ptr @load_pointer() {
; Z80-LABEL: load_pointer:
; Z80:         ld de,(_ptr)
; Z80-NEXT:    ret
  %v = load ptr, ptr @ptr
  ret ptr %v
}

; Read-modify-write reaches the global twice from one G_GLOBAL_VALUE. Both uses
; fold, which leaves the address materialization dead.
define void @accumulate(i16 %delta) {
; Z80-LABEL: accumulate:
; Z80-NOT:     ld hl,_counter
; Z80:         ld bc,(_counter)
; Z80-NEXT:    add hl,bc
; Z80-NEXT:    ld (_counter),hl
; Z80-NEXT:    ret
  %old = load i16, ptr @counter
  %new = add i16 %old, %delta
  store i16 %new, ptr @counter
  ret void
}

; Displacements sum at the width of a pointer. The combiner folds the chain
; that way whenever it runs, so the -O0 line is the one that would otherwise
; put an out-of-range addend in the relocation: 30000 three times over is
; 90000, which wraps to 24464.
define i16 @wrapping_displacement() {
; Z80-LABEL: wrapping_displacement:
; Z80:         ld de,(_big+24464)
; Z80-NEXT:    ret
  %p1 = getelementptr i8, ptr @big, i16 30000
  %p2 = getelementptr i8, ptr %p1, i16 30000
  %p3 = getelementptr i8, ptr %p2, i16 30000
  %v = load i16, ptr %p3
  ret i16 %v
}

define i16 @negative_displacement() {
; Z80-LABEL: negative_displacement:
; Z80:         ld de,(_big-10)
; Z80-NEXT:    ret
  %p = getelementptr i8, ptr @big, i16 -10
  %v = load i16, ptr %p
  ret i16 %v
}

; An address that is not known at link time still goes through a pointer.
define i16 @load_indirect(ptr %p) {
; Z80-LABEL: load_indirect:
; Z80:         ld e,(hl)
; Z80-NEXT:    inc hl
; Z80-NEXT:    ld d,(hl)
  %v = load i16, ptr %p
  ret i16 %v
}
