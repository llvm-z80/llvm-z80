; RUN: llc -mtriple=sm83-unknown-none-sdcc < %s | FileCheck %s --check-prefix=SDCC \
; RUN:     --implicit-check-not=_INITIALIZER --implicit-check-not=_INITIALIZED \
; RUN:     --implicit-check-not='.globl _internal_buf'
; RUN: llc -mtriple=z80-unknown-none-sdcc < %s | FileCheck %s --check-prefix=SDCC \
; RUN:     --implicit-check-not=_INITIALIZER --implicit-check-not=_INITIALIZED \
; RUN:     --implicit-check-not='.globl _internal_buf'

; External zero array
@external_buf = global [64 x i16] zeroinitializer, align 1

; The SDCC environment should put these in _DATA
; SDCC:      .area _DATA
; SDCC:      .globl _external_buf
; SDCC-NEXT: _external_buf:
; SDCC-NEXT: .ds 128
; SDCC-NOT:  .db

; Internal zero array
@internal_buf = internal global [64 x i16] zeroinitializer, align 1

; SDCC-NEXT: _internal_buf:
; SDCC-NEXT: .ds 128
; SDCC-NOT:  .db
