; RUN: llc -mtriple=sm83-unknown-none-sdcc < %s | FileCheck %s --check-prefix=SDCC
; RUN: llc -mtriple=sm83-unknown-none -z80-asm-format=sdasz80 < %s | FileCheck %s --check-prefix=SYNTAX
; RUN: llc -mtriple=sm83-unknown-none < %s | FileCheck %s --check-prefix=ELF

; A writable byte whose initial value must survive startup.
@counter = global i8 42, align 1

; The SDCC environment requires separate storage and initialization data.
; SDCC:      .area _INITIALIZED
; SDCC:      _counter:
; SDCC-NEXT: .ds 1
; SDCC-NOT:  .db
; SDCC:      .area _INITIALIZER
; SDCC:      .db 42

; Selecting assembly syntax alone preserves the existing data model.
; SYNTAX:      .area _DATA
; SYNTAX:      _counter:
; SYNTAX-NEXT: .db 42

; Default ELF output preserves the ordinary initialized-data section.
; ELF:      .data
; ELF:      _counter:
; ELF-NEXT: .byte 42

; Read-only data stays in ROM.
@read_only = constant i8 7, align 1

; SDCC:      .area _CODE
; SDCC:      _read_only:
; SDCC-NEXT: .db 7
