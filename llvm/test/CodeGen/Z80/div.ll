; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O0 < %s | FileCheck %s

; Test: signed 16-bit division via library call
define i16 @sdiv16(i16 %a, i16 %b) {
; CHECK-LABEL: _sdiv16:
; CHECK:       call ___divhi3
  %r = sdiv i16 %a, %b
  ret i16 %r
}

; Test: unsigned 16-bit division via library call
define i16 @udiv16(i16 %a, i16 %b) {
; CHECK-LABEL: _udiv16:
; CHECK:       call ___udivhi3
  %r = udiv i16 %a, %b
  ret i16 %r
}

; Test: signed 16-bit remainder via library call
define i16 @srem16(i16 %a, i16 %b) {
; CHECK-LABEL: _srem16:
; CHECK:       call ___modhi3
  %r = srem i16 %a, %b
  ret i16 %r
}

; Test: unsigned 16-bit remainder via library call
define i16 @urem16(i16 %a, i16 %b) {
; CHECK-LABEL: _urem16:
; CHECK:       call ___umodhi3
  %r = urem i16 %a, %b
  ret i16 %r
}
