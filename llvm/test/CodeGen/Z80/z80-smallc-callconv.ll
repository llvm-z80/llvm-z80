; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O0 < %s | FileCheck %s
;
; SDCC has two distinct calling conventions with OPPOSITE argument order:
;   __sdcccall(0) (cc128): all args on stack, right-to-left push.
;   __smallc      (cc129): all args on stack, left-to-right push (first arg deepest).
; They are identical for a single argument.  z88dk's Small-C library is
; compiled __smallc.  Constants: 0x1111=4369, 0x2222=8738, 0x3333=13107.

declare cc128 i16 @f0(i16, i16, i16)
declare cc129 i16 @fs(i16, i16, i16)

; sdcccall(0): push 3rd, 2nd, 1st (first arg ends nearest the return address).
define void @call_sdcccall0() {
; CHECK-LABEL: _call_sdcccall0:
; CHECK:       ld hl,#13107
; CHECK:       push hl
; CHECK:       ld hl,#8738
; CHECK:       push hl
; CHECK:       ld hl,#4369
; CHECK:       push hl
; CHECK:       call _f0
  call cc128 i16 @f0(i16 4369, i16 8738, i16 13107)
  ret void
}

; __smallc: push 1st, 2nd, 3rd (first arg ends deepest, last at top).
define void @call_smallc() {
; CHECK-LABEL: _call_smallc:
; CHECK:       ld hl,#4369
; CHECK:       push hl
; CHECK:       ld hl,#8738
; CHECK:       push hl
; CHECK:       ld hl,#13107
; CHECK:       push hl
; CHECK:       call _fs
  call cc129 i16 @fs(i16 4369, i16 8738, i16 13107)
  ret void
}
