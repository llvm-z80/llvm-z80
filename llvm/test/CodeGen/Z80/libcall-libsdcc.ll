; RUN: llc -mtriple=z80-unknown-none-sdcc -O1 < %s | FileCheck %s --check-prefixes=CHECK,Z80
; RUN: llc -mtriple=sm83-nintendo-none-sdcc -O1 < %s | FileCheck %s --check-prefixes=CHECK,SM83

; The sdcc environment calls SDCC's own runtime, and compiler-rt for what it
; lacks.

; CHECK-LABEL: _mul16:
; CHECK: call __mulint
define i16 @mul16(i16 %a, i16 %b) {
  %r = mul i16 %a, %b
  ret i16 %r
}

; CHECK-LABEL: _rem16:
; CHECK: call __moduint
define i16 @rem16(i16 %a, i16 %b) {
  %r = urem i16 %a, %b
  ret i16 %r
}

; CHECK-LABEL: _div32:
; CHECK: call __divslong
define i32 @div32(i32 %a, i32 %b) {
  %r = sdiv i32 %a, %b
  ret i32 %r
}

; CHECK-LABEL: _mul64:
; CHECK: call __mullonglong
define i64 @mul64(i64 %a, i64 %b) {
  %r = mul i64 %a, %b
  ret i64 %r
}

; CHECK-LABEL: _fadd:
; CHECK: call ___fsadd
define float @fadd(float %a, float %b) {
  %r = fadd float %a, %b
  ret float %r
}

; CHECK-LABEL: _fastadd:
; CHECK: call ___fsadd
define float @fastadd(float %a, float %b) {
  %r = fadd nnan ninf nsz float %a, %b
  ret float %r
}

; SDCC's comparisons return a boolean: a > b is b < a, a >= b is !(a < b).
; CHECK-LABEL: _flt:
; CHECK: call ___fslt
define i1 @flt(float %a, float %b) {
  %r = fcmp olt float %a, %b
  ret i1 %r
}

; CHECK-LABEL: _fge:
; CHECK: call ___fslt
; CHECK-NEXT: sub #1
define i1 @fge(float %a, float %b) {
  %r = fcmp oge float %a, %b
  ret i1 %r
}

; CHECK-LABEL: _fne:
; CHECK: call ___fsneq
define i1 @fne(float %a, float %b) {
  %r = fcmp une float %a, %b
  ret i1 %r
}

; NaN is taken to be absent.
; CHECK-LABEL: _funo:
; CHECK-NOT: call
define i1 @funo(float %a, float %b) {
  %r = fcmp uno float %a, %b
  ret i1 %r
}

; CHECK-LABEL: _tofloat:
; CHECK: call ___slong2fs
define float @tofloat(i32 %a) {
  %r = sitofp i32 %a to float
  ret float %r
}

; CHECK-LABEL: _divrem:
; CHECK: call ___divmodhi4
define i16 @divrem(i16 %a, i16 %b) {
  %q = sdiv i16 %a, %b
  %m = srem i16 %a, %b
  %r = add i16 %q, %m
  ret i16 %r
}

; Z80-LABEL: _indirect:
; Z80: call ___sdcc_call_iy
; SM83-LABEL: _indirect:
; SM83: call ___sdcc_call_hl
define void @indirect(ptr %f) {
  call void %f()
  call void %f()
  ret void
}
