; RUN: llvm-mc -triple=z80-unknown-sdcc -filetype=obj %s -o - | llvm-readelf -S - | FileCheck %s --check-prefixes=CHECK,SDCC
; RUN: llvm-mc -triple=z80 -z80-asm-format=sdasz80 -filetype=obj %s -o - | llvm-readelf -S - | FileCheck %s --check-prefixes=CHECK,SYNTAX

; CHECK:  .text        PROGBITS {{[0-9a-f ]+}} AX
	.area	_CODE
	nop
	.area	_HOME
	nop
; SDCC:   _DATA        NOBITS   {{[0-9a-f ]+}} WA
; SYNTAX: .data        PROGBITS {{[0-9a-f ]+}} WA
	.area	_DATA
	.ds	1
; CHECK:  _INITIALIZED NOBITS   {{[0-9a-f ]+}} WA
	.area	_INITIALIZED
	.ds	1
; CHECK:  _INITIALIZER PROGBITS {{[0-9a-f ]+}} A {{[0-9]}}
	.area	_INITIALIZER
	.db	5
; CHECK:  .bss         NOBITS   {{[0-9a-f ]+}} WA
	.area	_BSS
	.ds	1
; CHECK:  _GSINIT      PROGBITS {{[0-9a-f ]+}} AX
	.area	_GSINIT
	nop
