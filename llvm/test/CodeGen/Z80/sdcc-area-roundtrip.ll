; RUN: llc -mtriple=z80-unknown-none-sdcc -filetype=obj < %s | llvm-readelf -S - | FileCheck %s
; RUN: llc -mtriple=z80-unknown-none-sdcc < %s \
; RUN:   | llvm-mc -triple=z80-unknown-none-sdcc -filetype=obj | llvm-readelf -S - | FileCheck %s

; llvm-mc reads the printed areas back into the sections that llc emits.

; CHECK: _DATA        NOBITS   {{[0-9a-f ]+}} WA
@zero = global i8 0
; CHECK: _INITIALIZED NOBITS   {{[0-9a-f ]+}} WA
; CHECK: _INITIALIZER PROGBITS {{[0-9a-f ]+}} A {{[0-9]}}
@init = global i8 5
