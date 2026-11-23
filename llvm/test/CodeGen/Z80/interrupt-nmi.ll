; RUN: llc -verify-machineinstrs -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s

; Test: an NMI handler returns with retn and does not enable interrupts

@g = global i8 0

define void @nmi() "interrupt"="nmi" {
; CHECK-LABEL: _nmi:
; CHECK-NOT:   ei
; CHECK:       push af
; CHECK:       pop af
; CHECK-NEXT:  retn
  store volatile i8 66, ptr @g
  ret void
}
