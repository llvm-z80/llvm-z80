; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O0 < %s | FileCheck %s

; 16-bit EQ: uses XOR-based comparison (does not clobber HL)
define i8 @icmp_eq16(i16 %a, i16 %b) {
; CHECK-LABEL: icmp_eq16:
; CHECK:       xor
; CHECK:       or b
; CHECK:       sub #1
; CHECK:       sbc a,a
; CHECK:       and #1
  %c = icmp eq i16 %a, %b
  %r = zext i1 %c to i8
  ret i8 %r
}

; 16-bit NE: uses XOR-based comparison
define i8 @icmp_ne16(i16 %a, i16 %b) {
; CHECK-LABEL: icmp_ne16:
; CHECK:       xor
; CHECK:       or b
; CHECK:       add a,#255
; CHECK:       sbc a,a
; CHECK:       and #1
  %c = icmp ne i16 %a, %b
  %r = zext i1 %c to i8
  ret i8 %r
}

; 16-bit ULT: uses 8-bit SUB/SBC chain (CMP16_FLAGS)
define i8 @icmp_ult16(i16 %a, i16 %b) {
; CHECK-LABEL: icmp_ult16:
; CHECK:       sub e
; CHECK:       sbc a,d
; CHECK:       sbc a,a
; CHECK:       and #1
  %c = icmp ult i16 %a, %b
  %r = zext i1 %c to i8
  ret i8 %r
}

; 16-bit UGE: uses 8-bit SUB/SBC chain + CCF
define i8 @icmp_uge16(i16 %a, i16 %b) {
; CHECK-LABEL: icmp_uge16:
; CHECK:       sub e
; CHECK:       sbc a,d
; CHECK:       ccf
; CHECK:       sbc a,a
; CHECK:       and #1
  %c = icmp uge i16 %a, %b
  %r = zext i1 %c to i8
  ret i8 %r
}
