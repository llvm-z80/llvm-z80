; RUN: llc -mtriple=z80 -verify-machineinstrs %s -o - | FileCheck %s
; RUN: llc -mtriple=sm83 -verify-machineinstrs %s -o - | FileCheck %s --check-prefix=SM83

target triple = "z80"

define i8 @and_from_ptr(ptr %p, i8 %x) {
entry:
  %mem = load i8, ptr %p, align 1
  %result = and i8 %mem, %x
  ret i8 %result
}

; CHECK-LABEL: _and_from_ptr:
; CHECK:       and (hl)
; SM83-LABEL:  _and_from_ptr:
; SM83:        and (hl)

define i8 @xor_from_ptr(ptr %p, i8 %x) {
entry:
  %mem = load i8, ptr %p, align 1
  %result = xor i8 %x, %mem
  ret i8 %result
}

; CHECK-LABEL: _xor_from_ptr:
; CHECK:       xor (hl)
; SM83-LABEL:  _xor_from_ptr:
; SM83:        xor (hl)

define i8 @add_from_ptr(ptr %p, i8 %x) {
entry:
  %mem = load i8, ptr %p, align 1
  %result = add i8 %mem, %x
  ret i8 %result
}

; CHECK-LABEL: _add_from_ptr:
; CHECK:       add a,(hl)
; SM83-LABEL:  _add_from_ptr:
; SM83:        add a,(hl)

define i8 @sub_from_ptr(i8 %x, ptr %p) {
entry:
  %mem = load i8, ptr %p, align 1
  %result = sub i8 %x, %mem
  ret i8 %result
}

; CHECK-LABEL: _sub_from_ptr:
; CHECK:       sub (hl)
; SM83-LABEL:  _sub_from_ptr:
; SM83:        sub (hl)

; SUB is non-commutative: when the pointer load is on the LHS, it cannot
; fold into SUB (HL) because SUB (HL) computes A - (HL), not (HL) - A.
define i8 @sub_lhs_ptr(ptr %p, i8 %x) {
entry:
  %mem = load i8, ptr %p, align 1
  %result = sub i8 %mem, %x
  ret i8 %result
}

; CHECK-LABEL: _sub_lhs_ptr:
; CHECK-NOT:   sub (hl)
; CHECK:       sub
