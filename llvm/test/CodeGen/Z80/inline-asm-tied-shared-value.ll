; RUN: llc -mtriple=sm83 -O0 -verify-machineinstrs < %s | FileCheck %s --check-prefix=SM83
; RUN: llc -mtriple=sm83 -O2 -verify-machineinstrs < %s | FileCheck %s --check-prefix=SM83
; RUN: llc -mtriple=z80 -O0 -verify-machineinstrs < %s | FileCheck %s --check-prefix=Z80
; RUN: llc -mtriple=z80 -O2 -verify-machineinstrs < %s | FileCheck %s --check-prefix=Z80
;
; The memset size and the asm input tied to BC are one constant. The class of
; the asm input must not reach the constant, or on SM83 the constant needs BC
; or DE at the memset call, where both pairs hold arguments.

declare void @llvm.memset.p0.i16(ptr, i8, i16, i1)

; SM83-LABEL: _f:
; SM83:       ld bc,0
; SM83-NEXT:  ld hl,16
; SM83-NEXT:  call ___z80_memset_builtin
; SM83-NEXT:  ld bc,16
; SM83-NEXT:  ;APP
; Z80-LABEL:  _f:
; Z80:        ld de,0
; Z80-NEXT:   ld bc,16
; Z80-NEXT:   call ___z80_memset_builtin
; Z80-NEXT:   ld bc,16
; Z80-NEXT:   ;APP
define i16 @f(ptr %p) {
  call void @llvm.memset.p0.i16(ptr %p, i8 0, i16 16, i1 false)
  %r = call i16 asm sideeffect "nop", "={bc},0"(i16 16)
  ret i16 %r
}
