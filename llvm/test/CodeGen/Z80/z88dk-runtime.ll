; Runtime calls in the z88dk environment.
; RUN: llc -mtriple=z80-unknown-none-z88dk -O1 < %s | FileCheck %s

; CHECK-LABEL: _mul16:
; CHECK:       call l_mulu_16_16x16
; CHECK-NEXT:  ret
define i16 @mul16(i16 %a, i16 %b) {
  %r = mul i16 %a, %b
  ret i16 %r
}

; CHECK-LABEL: _srem16:
; CHECK:       call l_divs_16_16x16
; CHECK-NEXT:  ex de,hl
; CHECK-NEXT:  ret
define i16 @srem16(i16 %a, i16 %b) {
  %r = srem i16 %a, %b
  ret i16 %r
}

; CHECK-LABEL: _divrem16:
; CHECK:       call l_divu_16_16x16
; CHECK-NOT:   call
; CHECK:       ret
define i16 @divrem16(i16 %a, i16 %b) {
  %q = udiv i16 %a, %b
  %r = urem i16 %a, %b
  %s = xor i16 %q, %r
  ret i16 %s
}

; CHECK-LABEL: _umulo16:
; CHECK:       call l_mulu_32_16x16
; CHECK-NEXT:  ld a,e
; CHECK-NEXT:  or d
define i1 @umulo16(i16 %a, i16 %b) {
  %r = call {i16, i1} @llvm.umul.with.overflow.i16(i16 %a, i16 %b)
  %o = extractvalue {i16, i1} %r, 1
  ret i1 %o
}

; A byte division is a call only at minsize.
; CHECK-LABEL: _udiv8:
; CHECK:       call l_fast_divu_8_8x8
; CHECK-NEXT:  ret
define i8 @udiv8(i8 %a, i8 %b) minsize {
  %r = udiv i8 %a, %b
  ret i8 %r
}

; CHECK-LABEL: _urem8:
; CHECK:       call l_fast_divu_8_8x8
; CHECK-NEXT:  ld l,e
; CHECK-NEXT:  ret
define i8 @urem8(i8 %a, i8 %b) minsize {
  %r = urem i8 %a, %b
  ret i8 %r
}

; CHECK-LABEL: _mul32:
; CHECK:       exx
; CHECK-NEXT:  pop hl
; CHECK-NEXT:  pop de
; CHECK-NEXT:  push ix
; CHECK-NEXT:  call l_mulu_32_32x32
; CHECK-NEXT:  pop ix
; CHECK-NEXT:  ret
define i32 @mul32(i32 %a, i32 %b) {
  %r = mul i32 %a, %b
  ret i32 %r
}

; CHECK-LABEL: _srem32:
; CHECK:       call l_divs_32_32x32
; CHECK-NEXT:  pop ix
; CHECK-NEXT:  exx
; CHECK-NEXT:  ret
define i32 @srem32(i32 %a, i32 %b) {
  %r = srem i32 %a, %b
  ret i32 %r
}

; The remainder goes through the pointer in BC.
; CHECK-LABEL: _divrem32:
; CHECK:       pop iy
; CHECK-NEXT:  exx
; CHECK-NEXT:  pop hl
; CHECK-NEXT:  pop de
; CHECK-NEXT:  push ix
; CHECK-NEXT:  call l_divu_32_32x32
; CHECK-NEXT:  pop ix
; CHECK-NEXT:  exx
; CHECK-NEXT:  ld (iy+0),l
; CHECK-NEXT:  ld (iy+1),h
; CHECK-NEXT:  ld (iy+2),e
; CHECK-NEXT:  ld (iy+3),d
; CHECK-NEXT:  exx
define i32 @divrem32(i32 %a, i32 %b) {
  %q = udiv i32 %a, %b
  %r = urem i32 %a, %b
  %s = xor i32 %q, %r
  ret i32 %s
}

; The routine pops the left operand itself.
; CHECK-LABEL: _fadd:
; CHECK:       call l_f32_add
; CHECK-NEXT:  ret
define float @fadd(float %a, float %b) {
  %r = fadd float %a, %b
  ret float %r
}

; CHECK-LABEL: _flt:
; CHECK:       call l_f32_ge
; CHECK-NEXT:  jr c,
define i16 @flt(float %a, float %b) {
  %c = fcmp olt float %a, %b
  br i1 %c, label %t, label %f
t:
  ret i16 1
f:
  ret i16 2
}

; CHECK-LABEL: _sitofp:
; CHECK:       call l_f32_slong2f
; CHECK-NEXT:  ret
define float @sitofp(i32 %a) {
  %r = sitofp i32 %a to float
  ret float %r
}

; CHECK-LABEL: _floorf:
; CHECK:       call _floor
; CHECK-NEXT:  pop af
; CHECK-NEXT:  pop af
define float @floorf(float %a) {
  %r = call float @llvm.floor.f32(float %a)
  ret float %r
}

; CHECK-LABEL: _mul64:
; CHECK:       call ___muldi3
define i64 @mul64(i64 %a, i64 %b) {
  %r = mul i64 %a, %b
  ret i64 %r
}

; CHECK-LABEL: _icall:
; CHECK:       pop iy
; CHECK-NEXT:  call l_jpiy
define i16 @icall(ptr %f) {
  %r = call i16 %f()
  ret i16 %r
}

; CHECK-DAG: EXTERN l_mulu_16_16x16
; CHECK-DAG: EXTERN _floor
; CHECK-DAG: EXTERN ___muldi3
