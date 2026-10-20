; RUN: llc -mtriple=sm83-unknown-none-sdcc < %s | FileCheck %s --check-prefix=SDCC

@a = global i16 1, align 1

; Reserve RAM for a
; SDCC:      .area _INITIALIZED
; SDCC:      _a:
; SDCC:      .ds 2
; Store its initial value
; SDCC:      .area _INITIALIZER
; SDCC:      .dw 1

@tbl = constant [2 x i8] [i8 7, i8 8], align 1, section "_CODE_1"

; Emit tbl separately
; SDCC:      .area _CODE_1
; SDCC:      _tbl:
; SDCC:      .db 7
; SDCC-NEXT: .db 8

@b = global i16 2, align 1

; Reserve RAM for b
; SDCC:      .area _INITIALIZED
; SDCC:      _b:
; SDCC:      .ds 2
; SDCC:      .area _INITIALIZER
; SDCC:      .dw 2
