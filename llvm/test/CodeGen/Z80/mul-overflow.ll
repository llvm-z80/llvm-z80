; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s

declare {i16, i1} @llvm.umul.with.overflow.i16(i16, i16)
declare {i16, i1} @llvm.smul.with.overflow.i16(i16, i16)

; Test: unsigned multiply with overflow detection
; Should use __umulhi3 (upper half) and __mulhi3 (lower half)
define i16 @test_umulo(i16 %a, i16 %b) {
; CHECK-LABEL: _test_umulo:
; CHECK:       call ___umulhi3
; CHECK:       call ___mulhi3
; CHECK:       ret
  %r = call {i16, i1} @llvm.umul.with.overflow.i16(i16 %a, i16 %b)
  %val = extractvalue {i16, i1} %r, 0
  %ovf = extractvalue {i16, i1} %r, 1
  %res = select i1 %ovf, i16 0, i16 %val
  ret i16 %res
}

; Test: signed multiply with overflow detection
; Custom G_SMULH uses umulh + sign correction, so __umulhi3 comes first.
define i16 @test_smulo(i16 %a, i16 %b) {
; CHECK-LABEL: _test_smulo:
; CHECK:       call ___umulhi3
; CHECK:       call ___mulhi3
; CHECK:       ret
  %r = call {i16, i1} @llvm.smul.with.overflow.i16(i16 %a, i16 %b)
  %val = extractvalue {i16, i1} %r, 0
  %ovf = extractvalue {i16, i1} %r, 1
  %res = select i1 %ovf, i16 0, i16 %val
  ret i16 %res
}
