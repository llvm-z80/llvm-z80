; RUN: llc -verify-machineinstrs -mtriple=z80 -z80-asm-format=sdasz80 -O0 < %s | FileCheck %s
; RUN: llc -verify-machineinstrs -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s
;
; Test FCMP ordered and unordered predicates.
; Ordered predicates return false for NaN, unordered return true.

; Ordered EQ: single __eqsf2 call
define i8 @fcmp_oeq(float %a, float %b) {
; CHECK-LABEL: fcmp_oeq:
; CHECK:       call ___eqsf2
; CHECK-NOT:   call ___unordsf2
  %c = fcmp oeq float %a, %b
  %r = zext i1 %c to i8
  ret i8 %r
}

; Unordered EQ: __eqsf2 + __unordsf2, OR result
define i8 @fcmp_ueq(float %a, float %b) {
; CHECK-LABEL: fcmp_ueq:
; CHECK:       call ___eqsf2
; CHECK:       call ___unordsf2
; CHECK:       or
  %c = fcmp ueq float %a, %b
  %r = zext i1 %c to i8
  ret i8 %r
}

; Ordered NE: __nesf2 + __unordsf2, AND result
define i8 @fcmp_one(float %a, float %b) {
; CHECK-LABEL: fcmp_one:
; CHECK:       call ___nesf2
; CHECK:       call ___unordsf2
; CHECK:       and
  %c = fcmp one float %a, %b
  %r = zext i1 %c to i8
  ret i8 %r
}

; Unordered NE: single __nesf2 call (no __unordsf2 needed)
define i8 @fcmp_une(float %a, float %b) {
; CHECK-LABEL: fcmp_une:
; CHECK:       call ___nesf2
; CHECK-NOT:   call ___unordsf2
  %c = fcmp une float %a, %b
  %r = zext i1 %c to i8
  ret i8 %r
}

; ORD: __unordsf2 only, result == 0
define i8 @fcmp_ord(float %a, float %b) {
; CHECK-LABEL: fcmp_ord:
; CHECK:       call ___unordsf2
; CHECK-NOT:   call ___{{eq|ne|lt|le|gt|ge}}sf2
  %c = fcmp ord float %a, %b
  %r = zext i1 %c to i8
  ret i8 %r
}

; UNO: __unordsf2 only, result != 0
define i8 @fcmp_uno(float %a, float %b) {
; CHECK-LABEL: fcmp_uno:
; CHECK:       call ___unordsf2
; CHECK-NOT:   call ___{{eq|ne|lt|le|gt|ge}}sf2
  %c = fcmp uno float %a, %b
  %r = zext i1 %c to i8
  ret i8 %r
}

; Ordered GT: single __gtsf2 call
define i8 @fcmp_ogt(float %a, float %b) {
; CHECK-LABEL: fcmp_ogt:
; CHECK:       call ___gtsf2
; CHECK-NOT:   call ___unordsf2
  %c = fcmp ogt float %a, %b
  %r = zext i1 %c to i8
  ret i8 %r
}

; Unordered GT: __gtsf2 + __unordsf2
define i8 @fcmp_ugt(float %a, float %b) {
; CHECK-LABEL: fcmp_ugt:
; CHECK:       call ___gtsf2
; CHECK:       call ___unordsf2
  %c = fcmp ugt float %a, %b
  %r = zext i1 %c to i8
  ret i8 %r
}

; nnan alone drops the NaN test.
define i8 @fcmp_ult_nnan(float %a, float %b) {
; CHECK-LABEL: fcmp_ult_nnan:
; CHECK:       call ___ltsf2
; CHECK-NOT:   call ___unordsf2
  %c = fcmp nnan ult float %a, %b
  %r = zext i1 %c to i8
  ret i8 %r
}
