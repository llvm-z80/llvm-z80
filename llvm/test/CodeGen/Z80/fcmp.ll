; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O0 < %s | FileCheck %s
; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s
;
; Test FCMP ordered and unordered predicates.
; Ordered predicates return false for NaN, unordered return true.

; Ordered EQ: single __cmpsf2 call
define i8 @fcmp_oeq(float %a, float %b) {
; CHECK-LABEL: fcmp_oeq:
; CHECK:       call ___cmpsf2
; CHECK-NOT:   call ___unordsf2
  %c = fcmp oeq float %a, %b
  %r = zext i1 %c to i8
  ret i8 %r
}

; Unordered EQ: __cmpsf2 + __unordsf2, OR result
define i8 @fcmp_ueq(float %a, float %b) {
; CHECK-LABEL: fcmp_ueq:
; CHECK:       call ___cmpsf2
; CHECK:       call ___unordsf2
; CHECK:       or
  %c = fcmp ueq float %a, %b
  %r = zext i1 %c to i8
  ret i8 %r
}

; Ordered NE: __cmpsf2 + __unordsf2, AND result
define i8 @fcmp_one(float %a, float %b) {
; CHECK-LABEL: fcmp_one:
; CHECK:       call ___cmpsf2
; CHECK:       call ___unordsf2
; CHECK:       and
  %c = fcmp one float %a, %b
  %r = zext i1 %c to i8
  ret i8 %r
}

; Unordered NE: single __cmpsf2 call (no __unordsf2 needed)
define i8 @fcmp_une(float %a, float %b) {
; CHECK-LABEL: fcmp_une:
; CHECK:       call ___cmpsf2
; CHECK-NOT:   call ___unordsf2
  %c = fcmp une float %a, %b
  %r = zext i1 %c to i8
  ret i8 %r
}

; ORD: __unordsf2 only, result == 0
define i8 @fcmp_ord(float %a, float %b) {
; CHECK-LABEL: fcmp_ord:
; CHECK:       call ___unordsf2
; CHECK-NOT:   call ___cmpsf2
  %c = fcmp ord float %a, %b
  %r = zext i1 %c to i8
  ret i8 %r
}

; UNO: __unordsf2 only, result != 0
define i8 @fcmp_uno(float %a, float %b) {
; CHECK-LABEL: fcmp_uno:
; CHECK:       call ___unordsf2
; CHECK-NOT:   call ___cmpsf2
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
