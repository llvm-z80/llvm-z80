; RUN: llc -mtriple=z80 -O2 -verify-machineinstrs < %s | FileCheck %s --check-prefix=Z80
; RUN: llc -mtriple=sm83 -O2 -verify-machineinstrs < %s | FileCheck %s --check-prefix=SM83
;
; A body that pops its stack arguments on return moves the return address
; through a register pair. The pair is never a listed register or the result.

; Z80-LABEL: _keep_bc:
; Z80:       pop hl
; Z80-NEXT:  pop af
; Z80-NEXT:  pop af
; Z80-NEXT:  jp (hl)
; SM83-LABEL: _keep_bc:
; SM83:       pop hl
; SM83-NEXT:  add sp,4
; SM83-NEXT:  jp (hl)
define void @keep_bc(i8 %a, i8 %b, i16 %c, i16 %d) "z80-preserves-regs"="b,c" {
  ret void
}

; Z80-LABEL: _keep_hl:
; Z80:       pop bc
; Z80-NEXT:  ex de,hl
; Z80-NEXT:  ld hl,10
; Z80-NEXT:  add hl,sp
; Z80-NEXT:  ld sp,hl
; Z80-NEXT:  ex de,hl
; Z80-NEXT:  push bc
; Z80-NEXT:  ret
; SM83-LABEL: _keep_hl:
; SM83:       pop bc
; SM83-NEXT:  add sp,10
; SM83-NEXT:  push bc
; SM83-NEXT:  ret
define void @keep_hl(i16 %a, i16 %b, i16 %c, i16 %d, i16 %e, i16 %f, i16 %g) "z80-preserves-regs"="h,l" {
  ret void
}

; Z80 returns an i16 in DE and SM83 in BC.
; Z80-LABEL: _keep_ret16:
; Z80:       pop af
; Z80-NEXT:  inc sp
; Z80-NEXT:  inc sp
; Z80-NEXT:  inc sp
; Z80-NEXT:  inc sp
; Z80-NEXT:  push af
; Z80-NEXT:  ret
; SM83-LABEL: _keep_ret16:
; SM83:       pop de
; SM83-NEXT:  add sp,4
; SM83-NEXT:  push de
; SM83-NEXT:  ret
define i16 @keep_ret16(i8 %a, i8 %b, i16 %c, i16 %d) "z80-preserves-regs"="b,h" {
  ret i16 7
}

; Z80-LABEL: _keep_abh:
; Z80:       pop iy
; Z80-NEXT:  inc sp
; Z80-NEXT:  inc sp
; Z80-NEXT:  inc sp
; Z80-NEXT:  inc sp
; Z80-NEXT:  push iy
; Z80-NEXT:  ret
define i16 @keep_abh(i8 %a, i8 %b, i16 %c, i16 %d) "z80-preserves-regs"="a,b,h" {
  ret i16 7
}

; Z80 returns the float in HL and DE, and IY is listed. So the arguments go
; through POP AF.
; Z80-LABEL: _keep_iy:
; Z80:       pop bc
; Z80-NEXT:  pop af
; Z80-NEXT:  pop af
; Z80-NEXT:  pop af
; Z80-NEXT:  pop af
; Z80-NEXT:  pop af
; Z80-NEXT:  pop af
; Z80-NEXT:  push bc
; Z80-NEXT:  ret
define float @keep_iy(float %a, float %b, float %c, float %d) "z80-preserves-regs"="iyl,iyh" {
  ret float %a
}
