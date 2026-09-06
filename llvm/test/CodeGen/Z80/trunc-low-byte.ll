; RUN: llc -verify-machineinstrs -mtriple=z80 -O1 < %s -o /dev/null
; RUN: llc -verify-machineinstrs -mtriple=sm83 -O1 < %s -o /dev/null
; RUN: llc -mtriple=z80 -O1 -stop-after=instruction-select < %s -o - | FileCheck %s

; Taking the low byte of a pair names the half the value already occupies. A
; move into a fixed pair would leave a definition of that pair which the value
; outlives, so once the value lands in the same pair, reading its other half
; reads a register that definition had ended.

; CHECK-LABEL: name: low_byte
; CHECK: COPY %{{[0-9]+}}.sub_lo
; CHECK-NOT: $hl = COPY
; CHECK-LABEL: name: read_both_halves

define i8 @low_byte(i16 %c) {
entry:
  %t = trunc i16 %c to i8
  %m = and i8 %t, 7
  ret i8 %m
}

; Both halves of the same value are read after the low byte is taken, which is
; what puts the value and the pair it was moved into on the same register.

define i16 @read_both_halves(i16 %c) {
entry:
  %and = and i16 %c, 1
  %other = and i16 %c, 13962
  %zero = icmp eq i16 %other, 0
  br label %loop

loop:
  %done = icmp eq i16 %and, 0
  br i1 %done, label %loop, label %tail

tail:
  br i1 %zero, label %exit, label %loop

exit:
  ret i16 0
}
