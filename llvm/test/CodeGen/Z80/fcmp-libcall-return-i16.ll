; RUN: llc -mtriple=z80 -stop-after=legalizer < %s | FileCheck %s --check-prefix=Z80
; RUN: llc -mtriple=sm83 -stop-after=legalizer < %s | FileCheck %s --check-prefix=SM83

; Soft-float comparison libcalls return a 16-bit int, so the caller must not
; read the register pair that would hold the high half of an i32.

define i16 @deq(double %a, double %b) {
; Z80-LABEL: name: deq
; Z80:       CALL_nn &__eqdf2
; Z80:       [[RET:%[0-9]+]]:_(s16) = COPY $de
; Z80-NOT:   COPY $hl
; Z80:       G_ICMP intpred(eq), [[RET]](s16)
;
; SM83-LABEL: name: deq
; SM83:       CALL_nn &__eqdf2
; SM83:       [[RET:%[0-9]+]]:_(s16) = COPY $bc
; SM83-NOT:   COPY $de
; SM83:       G_ICMP intpred(eq), [[RET]](s16)
  %c = fcmp oeq double %a, %b
  %z = zext i1 %c to i16
  ret i16 %z
}
