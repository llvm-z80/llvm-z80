; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s

; Test: i128 addition (narrowed to 8 x i16 add/adc chain)
define i128 @add128(i128 %a, i128 %b) {
; CHECK-LABEL: _add128:
; CHECK:       add hl,
; CHECK:       adc hl,
; CHECK:       ret
  %r = add i128 %a, %b
  ret i128 %r
}

; Test: i128 subtraction
define i128 @sub128(i128 %a, i128 %b) {
; CHECK-LABEL: _sub128:
; CHECK:       ret
  %r = sub i128 %a, %b
  ret i128 %r
}

; Test: i128 bitwise AND
define i128 @and128(i128 %a, i128 %b) {
; CHECK-LABEL: _and128:
; CHECK:       and
; CHECK:       ret
  %r = and i128 %a, %b
  ret i128 %r
}

; Test: i128 bitwise OR
define i128 @or128(i128 %a, i128 %b) {
; CHECK-LABEL: _or128:
; CHECK:       or
; CHECK:       ret
  %r = or i128 %a, %b
  ret i128 %r
}

; Test: i128 bitwise XOR
define i128 @xor128(i128 %a, i128 %b) {
; CHECK-LABEL: _xor128:
; CHECK:       xor
; CHECK:       ret
  %r = xor i128 %a, %b
  ret i128 %r
}

; Test: i128 equality comparison
define i1 @eq128(i128 %a, i128 %b) {
; CHECK-LABEL: _eq128:
; CHECK:       ret
  %r = icmp eq i128 %a, %b
  ret i1 %r
}

; Test: i128 inequality comparison
define i1 @ne128(i128 %a, i128 %b) {
; CHECK-LABEL: _ne128:
; CHECK:       ret
  %r = icmp ne i128 %a, %b
  ret i1 %r
}

; Test: i128 signed less-than comparison
define i1 @slt128(i128 %a, i128 %b) {
; CHECK-LABEL: _slt128:
; CHECK:       ret
  %r = icmp slt i128 %a, %b
  ret i1 %r
}

; Test: i128 unsigned less-than comparison
define i1 @ult128(i128 %a, i128 %b) {
; CHECK-LABEL: _ult128:
; CHECK:       ret
  %r = icmp ult i128 %a, %b
  ret i1 %r
}

; Test: i128 multiplication (narrowed to 64-bit multiplies)
define i128 @mul128(i128 %a, i128 %b) {
; CHECK-LABEL: _mul128:
; CHECK:       call ___muldi3
; CHECK:       ret
  %r = mul i128 %a, %b
  ret i128 %r
}

; Test: i128 unsigned division (libcall)
define i128 @udiv128(i128 %a, i128 %b) {
; CHECK-LABEL: _udiv128:
; CHECK:       call ___udivti3
; CHECK:       ret
  %r = udiv i128 %a, %b
  ret i128 %r
}

; Test: i128 signed division (libcall)
define i128 @sdiv128(i128 %a, i128 %b) {
; CHECK-LABEL: _sdiv128:
; CHECK:       call ___divti3
; CHECK:       ret
  %r = sdiv i128 %a, %b
  ret i128 %r
}

; Test: i128 unsigned remainder (libcall)
define i128 @urem128(i128 %a, i128 %b) {
; CHECK-LABEL: _urem128:
; CHECK:       call ___umodti3
; CHECK:       ret
  %r = urem i128 %a, %b
  ret i128 %r
}

; Test: i128 zero extension from i64
define i128 @zext64to128(i64 %x) {
; CHECK-LABEL: _zext64to128:
; CHECK:       ret
  %r = zext i64 %x to i128
  ret i128 %r
}

; Test: i128 truncation to i64
define i64 @trunc128to64(i128 %x) {
; CHECK-LABEL: _trunc128to64:
; CHECK:       ret
  %r = trunc i128 %x to i64
  ret i64 %r
}
