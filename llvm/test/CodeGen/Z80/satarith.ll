; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s
; RUN: llc -mtriple=sm83 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s --check-prefix=SM83

; Test i8 saturating arithmetic — should use flag-based sequences,
; not generic comparison+select lowering.

define i8 @uaddsat_i8(i8 %a, i8 %b) {
; CHECK-LABEL: _uaddsat_i8:
; CHECK:       add a,
; CHECK:       jr nc,
; CHECK:       ld a,#255
; SM83-LABEL: _uaddsat_i8:
; SM83:       add a,
; SM83:       jr nc,
; SM83:       ld a,#255
  %r = call i8 @llvm.uadd.sat.i8(i8 %a, i8 %b)
  ret i8 %r
}

define i8 @usubsat_i8(i8 %a, i8 %b) {
; CHECK-LABEL: _usubsat_i8:
; CHECK:       sub
; CHECK:       jr nc,
; CHECK:       xor a
; SM83-LABEL: _usubsat_i8:
; SM83:       sub
; SM83:       jr nc,
; SM83:       xor a
  %r = call i8 @llvm.usub.sat.i8(i8 %a, i8 %b)
  ret i8 %r
}

define i8 @saddsat_i8(i8 %a, i8 %b) {
; CHECK-LABEL: _saddsat_i8:
; CHECK:       add a,
; CHECK:       jp po,
; CHECK:       rlca
; CHECK-NEXT:  sbc a,a
; CHECK-NEXT:  xor #128
  %r = call i8 @llvm.sadd.sat.i8(i8 %a, i8 %b)
  ret i8 %r
}

define i8 @ssubsat_i8(i8 %a, i8 %b) {
; CHECK-LABEL: _ssubsat_i8:
; CHECK:       sub
; CHECK:       jp po,
; CHECK:       rlca
; CHECK-NEXT:  sbc a,a
; CHECK-NEXT:  xor #128
  %r = call i8 @llvm.ssub.sat.i8(i8 %a, i8 %b)
  ret i8 %r
}

declare i8 @llvm.uadd.sat.i8(i8, i8)
declare i8 @llvm.usub.sat.i8(i8, i8)
declare i8 @llvm.sadd.sat.i8(i8, i8)
declare i8 @llvm.ssub.sat.i8(i8, i8)
