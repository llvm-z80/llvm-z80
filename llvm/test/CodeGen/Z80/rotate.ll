; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s

; Test G_FSHL/G_FSHR with same operands → native rotation (RLCA/RRCA)

declare i8 @llvm.fshl.i8(i8, i8, i8)
declare i8 @llvm.fshr.i8(i8, i8, i8)

; Variable rotation: uses DJNZ loop with RLCA
define i8 @rotl(i8 %x, i8 %amt) {
; CHECK-LABEL: _rotl:
; CHECK: rlca
; CHECK: djnz
; CHECK: ret
  %r = call i8 @llvm.fshl.i8(i8 %x, i8 %x, i8 %amt)
  ret i8 %r
}

; Variable rotation: uses DJNZ loop with RRCA
define i8 @rotr(i8 %x, i8 %amt) {
; CHECK-LABEL: _rotr:
; CHECK: rrca
; CHECK: djnz
; CHECK: ret
  %r = call i8 @llvm.fshr.i8(i8 %x, i8 %x, i8 %amt)
  ret i8 %r
}

; Constant rotation left by 1: single RLCA
define i8 @rotl_1(i8 %x) {
; CHECK-LABEL: _rotl_1:
; CHECK: rlca
; CHECK-NOT: rlca
; CHECK: ret
  %r = call i8 @llvm.fshl.i8(i8 %x, i8 %x, i8 1)
  ret i8 %r
}

; Constant rotation right by 1: single RRCA
define i8 @rotr_1(i8 %x) {
; CHECK-LABEL: _rotr_1:
; CHECK: rrca
; CHECK-NOT: rrca
; CHECK: ret
  %r = call i8 @llvm.fshr.i8(i8 %x, i8 %x, i8 1)
  ret i8 %r
}

; True funnel shift (different operands): lowered to shift+or
define i8 @fshl_diff(i8 %a, i8 %b) {
; CHECK-LABEL: _fshl_diff:
; CHECK: or
; CHECK: ret
  %r = call i8 @llvm.fshl.i8(i8 %a, i8 %b, i8 3)
  ret i8 %r
}
