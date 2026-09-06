; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O0 < %s | FileCheck %s
;
; SDCC has two distinct stack calling conventions with OPPOSITE argument order:
;   __sdcccall(0) (cc128): right-to-left push -> first arg nearest return addr.
;   __smallc      (cc132): left-to-right push -> last  arg nearest return addr.
; They are identical for a single argument.  z88dk's classic C library is
; compiled __smallc, so clang needs cc132 to call it correctly (ravn/llvm-z80#279,
; ravn/z88dk#41).  Constants: 0x1111=4369, 0x2222=8738, 0x3333=13107.

declare cc128 i16 @f0(i16, i16, i16)
declare cc132 i16 @fs(i16, i16, i16)

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

; __smallc: push 1st, 2nd, 3rd (last arg ends nearest the return address).
define void @call_smallc() {
; CHECK-LABEL: _call_smallc:
; CHECK:       ld hl,#4369
; CHECK:       push hl
; CHECK:       ld hl,#8738
; CHECK:       push hl
; CHECK:       ld hl,#13107
; CHECK:       push hl
; CHECK:       call _fs
  call cc132 i16 @fs(i16 4369, i16 8738, i16 13107)
  ret void
}
