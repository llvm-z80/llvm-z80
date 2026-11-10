; RUN: llc -mtriple=sm83-unknown-none-sdcc < %s | FileCheck %s --check-prefix=SDCC \
; RUN:     --implicit-check-not=_BSS

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

; SDCC's crt0 does not clear _BSS. Globals in .data and .bss sections go to
; _DATA and _INITIALIZED instead.
@d = global i16 3, align 1, section ".data.d"

; SDCC:      .area _INITIALIZED
; SDCC:      _d:
; SDCC:      .ds 2
; SDCC:      .area _INITIALIZER
; SDCC:      .dw 3

@s = global i16 0, align 1, section ".bss.s"

; SDCC:      .area _DATA
; SDCC:      _s:
; SDCC-NEXT: .ds 2

; A comdat global goes to _DATA too.
$c = comdat any
@c = linkonce_odr global i16 0, align 1, comdat

; SDCC:      _c:
; SDCC-NEXT: .ds 2
