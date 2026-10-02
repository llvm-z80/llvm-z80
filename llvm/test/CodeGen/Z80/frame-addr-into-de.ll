; RUN: llc -mtriple=z80 -O1 -verify-machineinstrs < %s | FileCheck %s
;
; The address of a stack object is computed in HL and moved into DE with
; EX DE,HL, saving HL around it while HL holds a live value. The swap reads
; DE, which is the register being defined and holds nothing yet.

%struct.foo = type { i16, i16 }

define i16 @addr_to_de(i16 %d, ptr byval(%struct.foo) align 1 %u) {
; CHECK-LABEL: _addr_to_de:
; CHECK:       push hl
; CHECK-NEXT:  ld hl,4
; CHECK-NEXT:  add hl,sp
; CHECK-NEXT:  ex de,hl
; CHECK-NEXT:  pop hl
  store i16 %d, ptr %u, align 1
  %a = ptrtoint ptr %u to i16
  ret i16 %a
}
