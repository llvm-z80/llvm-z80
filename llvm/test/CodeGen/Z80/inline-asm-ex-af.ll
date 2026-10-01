; RUN: llc -mtriple=z80 < %s | FileCheck %s
;
; Inline assembly takes af' the same way.

define void @swap() {
; CHECK-LABEL: _swap:
; CHECK:       ex af,af'
; CHECK-NEXT:  exx
; CHECK-NEXT:  ex af,af'
  call void asm sideeffect "ex af,af'\0A\09exx\0A\09EX AF, AF'", ""()
  ret void
}
