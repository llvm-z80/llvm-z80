; RUN: llc -mtriple=z80 -O1 < %s | FileCheck %s
; RUN: llc -mtriple=sm83 -O1 < %s | FileCheck %s --check-prefix=SM83

; Fixed-point arithmetic should compile without errors.
; Operations are custom-lowered to widen+mul/div+shift sequences.

declare i16 @llvm.smul.fix.i16(i16, i16, i32)
declare i16 @llvm.umul.fix.i16(i16, i16, i32)
declare i16 @llvm.smul.fix.sat.i16(i16, i16, i32)
declare i16 @llvm.umul.fix.sat.i16(i16, i16, i32)
declare i16 @llvm.sdiv.fix.i16(i16, i16, i32)
declare i16 @llvm.udiv.fix.i16(i16, i16, i32)
declare i8 @llvm.smul.fix.i8(i8, i8, i32)

define i16 @smulfix(i16 %a, i16 %b) nounwind {
; CHECK-LABEL: smulfix:
; CHECK-NOT:   G_SMULFIX
; CHECK:       ret
; SM83-LABEL: smulfix:
; SM83:       ret
  %r = call i16 @llvm.smul.fix.i16(i16 %a, i16 %b, i32 8)
  ret i16 %r
}

define i16 @umulfix(i16 %a, i16 %b) nounwind {
; CHECK-LABEL: umulfix:
; CHECK:       ret
; SM83-LABEL: umulfix:
; SM83:       ret
  %r = call i16 @llvm.umul.fix.i16(i16 %a, i16 %b, i32 8)
  ret i16 %r
}

define i16 @smulfixsat(i16 %a, i16 %b) nounwind {
; CHECK-LABEL: smulfixsat:
; CHECK:       ret
; SM83-LABEL: smulfixsat:
; SM83:       ret
  %r = call i16 @llvm.smul.fix.sat.i16(i16 %a, i16 %b, i32 8)
  ret i16 %r
}

define i16 @sdivfix(i16 %a, i16 %b) nounwind {
; CHECK-LABEL: sdivfix:
; CHECK:       ret
; SM83-LABEL: sdivfix:
; SM83:       ret
  %r = call i16 @llvm.sdiv.fix.i16(i16 %a, i16 %b, i32 8)
  ret i16 %r
}

define i16 @udivfix(i16 %a, i16 %b) nounwind {
; CHECK-LABEL: udivfix:
; CHECK:       ret
; SM83-LABEL: udivfix:
; SM83:       ret
  %r = call i16 @llvm.udiv.fix.i16(i16 %a, i16 %b, i32 4)
  ret i16 %r
}

define i8 @smulfix_i8(i8 %a, i8 %b) nounwind {
; CHECK-LABEL: smulfix_i8:
; CHECK:       ret
; SM83-LABEL: smulfix_i8:
; SM83:       ret
  %r = call i8 @llvm.smul.fix.i8(i8 %a, i8 %b, i32 4)
  ret i8 %r
}

define i16 @smulfixsat_scale0(i16 %a, i16 %b) nounwind {
; CHECK-LABEL: smulfixsat_scale0:
; CHECK:       ret
; SM83-LABEL: smulfixsat_scale0:
; SM83:       ret
  %r = call i16 @llvm.smul.fix.sat.i16(i16 %a, i16 %b, i32 0)
  ret i16 %r
}
