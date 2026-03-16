; RUN: llc -mtriple=z80 -O1 < %s | FileCheck %s
; RUN: llc -mtriple=sm83 -O1 < %s | FileCheck %s --check-prefix=SM83

; Test aggregate (struct) return packing into registers without stack slots.

define { i8, i8 } @ret_i8_i8() nounwind {
; CHECK-LABEL: ret_i8_i8:
; CHECK-NOT:   add hl,sp
; CHECK:       ret
; SM83-LABEL: ret_i8_i8:
; SM83-NOT:   add hl,sp
; SM83:       ret
  ret { i8, i8 } { i8 1, i8 2 }
}

define { i16, i16 } @ret_i16_i16() nounwind {
; CHECK-LABEL: ret_i16_i16:
; CHECK-NOT:   add hl,sp
; CHECK:       ret
; SM83-LABEL: ret_i16_i16:
; SM83-NOT:   add hl,sp
; SM83:       ret
  ret { i16, i16 } { i16 4660, i16 22136 }
}

define { i8, i16 } @ret_i8_i16() nounwind {
; CHECK-LABEL: ret_i8_i16:
; CHECK-NOT:   add hl,sp
; CHECK:       ret
; SM83-LABEL: ret_i8_i16:
; SM83-NOT:   add hl,sp
; SM83:       ret
  ret { i8, i16 } { i8 1, i16 4660 }
}

define { i16, i8 } @ret_i16_i8() nounwind {
; CHECK-LABEL: ret_i16_i8:
; CHECK-NOT:   add hl,sp
; CHECK:       ret
; SM83-LABEL: ret_i16_i8:
; SM83-NOT:   add hl,sp
; SM83:       ret
  ret { i16, i8 } { i16 4660, i8 5 }
}

define { i8, i8, i16 } @ret_i8_i8_i16() nounwind {
; CHECK-LABEL: ret_i8_i8_i16:
; CHECK-NOT:   add hl,sp
; CHECK:       ret
; SM83-LABEL: ret_i8_i8_i16:
; SM83-NOT:   add hl,sp
; SM83:       ret
  ret { i8, i8, i16 } { i8 1, i8 2, i16 4660 }
}

define { i8, i8, i8 } @ret_i8_i8_i8() nounwind {
; CHECK-LABEL: ret_i8_i8_i8:
; CHECK-NOT:   add hl,sp
; CHECK:       ret
; SM83-LABEL: ret_i8_i8_i8:
; SM83-NOT:   add hl,sp
; SM83:       ret
  ret { i8, i8, i8 } { i8 1, i8 2, i8 3 }
}

; Caller side: extract fields without stack round-trip.
declare { i8, i8 } @get_pair()

define i8 @use_pair() nounwind {
; CHECK-LABEL: use_pair:
; CHECK:       call
; CHECK-NOT:   add hl,sp
; CHECK:       ret
; SM83-LABEL: use_pair:
; SM83:       call
; SM83-NOT:   ldhl sp
; SM83:       ret
  %r = call { i8, i8 } @get_pair()
  %a = extractvalue { i8, i8 } %r, 0
  %b = extractvalue { i8, i8 } %r, 1
  %sum = add i8 %a, %b
  ret i8 %sum
}
