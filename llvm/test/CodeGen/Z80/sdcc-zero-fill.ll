; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 < %s | FileCheck %s
; RUN: llc -mtriple=sm83 -z80-asm-format=sdasz80 < %s | FileCheck %s

; sdasz80's .ds only moves the location counter along, so a section whose bytes
; are written out needs its zeros spelled out. Only .bss, which the linker
; leaves out of the image, can name the space and stop there.

@ro_zeros = constant [8 x i8] zeroinitializer
@ro_mixed = constant { [8 x i8], i16 } { [8 x i8] zeroinitializer, i16 1 }
@bss_zeros = global [64 x i8] zeroinitializer

; CHECK-LABEL: _ro_zeros:
; CHECK-NOT:     .ds
; CHECK-COUNT-8: .db 0
; CHECK-NOT:     .ds

; CHECK-LABEL: _ro_mixed:
; CHECK-COUNT-8: .db 0
; CHECK:         .dw 1

; CHECK:       .area _BSS
; CHECK-LABEL: _bss_zeros:
; CHECK:         .ds 64
