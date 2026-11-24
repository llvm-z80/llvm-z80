; RUN: llc -mtriple=z80 -stop-after=ir-translator < %s | FileCheck %s --check-prefix=Z80
; RUN: llc -mtriple=sm83 -stop-after=ir-translator < %s | FileCheck %s --check-prefix=SM83
;
; A call to a function that preserves registers keeps them in its mask, and a
; pair when both halves are kept. A pair name keeps both halves. A register
; that passes an argument or returns the result is not kept.

declare void @bc() "z80-preserves-regs"="b,c"
declare i8 @all(i8) "z80-preserves-regs"="a,b,c,d,e,h,l"
declare void @arg16(i16) "z80-preserves-regs"="b,h,l"
declare i16 @ret16() "z80-preserves-regs"="b,c,d,e"
declare z80_sdcccall0 i8 @cc0() "z80-preserves-regs"="h"
declare void @index() "z80-preserves-regs"="iyl,iyh,ixl"
declare void @pair() "z80-preserves-regs"="bc"
declare void @junk() "z80-preserves-regs"="x"

define void @calls() {
  call void @bc()
  %a = call i8 @all(i8 1)
  call void @arg16(i16 2)
  %b = call i16 @ret16()
  %c = call z80_sdcccall0 i8 @cc0()
  call void @index()
  call void @pair()
  call void @junk()
  ret void
}

; Z80-LABEL: name: calls
; Z80:  CALL_nn @bc, CustomRegMask($b,$bc,$c,$ix,$ixh,$ixl)
; Z80:  CALL_nn @all, CustomRegMask($b,$bc,$c,$d,$de,$e,$h,$hl,$ix,$ixh,$ixl,$l)
; Z80:  CALL_nn @arg16, CustomRegMask($b,$ix,$ixh,$ixl)
; Z80:  CALL_nn @ret16, CustomRegMask($b,$bc,$c,$ix,$ixh,$ixl)
; Z80:  CALL_nn @cc0, CustomRegMask($h,$ix,$ixh,$ixl)
; Z80:  CALL_nn @index, CustomRegMask($ix,$ixh,$ixl,$iy,$iyh,$iyl)
; Z80:  CALL_nn @pair, CustomRegMask($b,$bc,$c,$ix,$ixh,$ixl)
; Z80:  CALL_nn @junk, z80_csr

; SM83-LABEL: name: calls
; SM83: CALL_nn @bc, CustomRegMask($b,$bc,$c)
; SM83: CALL_nn @all, CustomRegMask($b,$bc,$c,$d,$de,$e,$h,$hl,$l)
; SM83: CALL_nn @arg16, CustomRegMask($b,$h,$hl,$l)
; SM83: CALL_nn @ret16, CustomRegMask($d,$de,$e)
; SM83: CALL_nn @cc0, CustomRegMask($h)
; SM83: CALL_nn @index, sm83_csr
; SM83: CALL_nn @pair, CustomRegMask($b,$bc,$c)
; SM83: CALL_nn @junk, sm83_csr
