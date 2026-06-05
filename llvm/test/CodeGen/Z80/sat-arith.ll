; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s

declare i16 @llvm.uadd.sat.i16(i16, i16)
declare i16 @llvm.usub.sat.i16(i16, i16)
declare i16 @llvm.sadd.sat.i16(i16, i16)
declare i16 @llvm.ssub.sat.i16(i16, i16)
declare i8 @llvm.scmp.i8.i16(i16, i16)
declare i8 @llvm.ucmp.i8.i16(i16, i16)

; Test: unsigned add saturating
define i16 @test_uaddsat(i16 %a, i16 %b) {
; CHECK-LABEL: _test_uaddsat:
; CHECK:       add hl,bc
; CHECK:       sbc a,a
; CHECK:       and #1
; CHECK:       ret
  %r = call i16 @llvm.uadd.sat.i16(i16 %a, i16 %b)
  ret i16 %r
}

; Test: unsigned sub saturating
define i16 @test_usubsat(i16 %a, i16 %b) {
; CHECK-LABEL: _test_usubsat:
; CHECK:       sbc hl,de
; CHECK:       sbc a,a
; CHECK:       and #1
; CHECK:       ret
  %r = call i16 @llvm.usub.sat.i16(i16 %a, i16 %b)
  ret i16 %r
}

; Test: signed add saturating (uses P/V flag capture via CAPTURE_PV pseudo)
define i16 @test_saddsat(i16 %a, i16 %b) {
; CHECK-LABEL: _test_saddsat:
; CHECK:       and a
; CHECK:       adc hl,de
; CHECK:       push af
; CHECK-NEXT:  pop hl
; CHECK-NEXT:  ld a,l
; CHECK-NEXT:  rrca
; CHECK-NEXT:  rrca
; CHECK-NEXT:  and #1
; CHECK:       ret
  %r = call i16 @llvm.sadd.sat.i16(i16 %a, i16 %b)
  ret i16 %r
}

; Test: signed sub saturating (uses P/V flag capture via CAPTURE_PV pseudo)
define i16 @test_ssubsat(i16 %a, i16 %b) {
; CHECK-LABEL: _test_ssubsat:
; CHECK:       and a
; CHECK:       sbc hl,de
; CHECK:       push af
; CHECK-NEXT:  pop hl
; CHECK-NEXT:  ld a,l
; CHECK-NEXT:  rrca
; CHECK-NEXT:  rrca
; CHECK-NEXT:  and #1
; CHECK:       ret
  %r = call i16 @llvm.ssub.sat.i16(i16 %a, i16 %b)
  ret i16 %r
}

; Test: three-way signed comparison
define i8 @test_scmp(i16 %a, i16 %b) {
; CHECK-LABEL: _test_scmp:
; CHECK:       sbc hl,de
; CHECK:       ret
  %r = call i8 @llvm.scmp.i8.i16(i16 %a, i16 %b)
  ret i8 %r
}

; Test: three-way unsigned comparison
define i8 @test_ucmp(i16 %a, i16 %b) {
; CHECK-LABEL: _test_ucmp:
; CHECK:       sub e
; CHECK:       sbc a,d
; CHECK:       ret
  %r = call i8 @llvm.ucmp.i8.i16(i16 %a, i16 %b)
  ret i8 %r
}
