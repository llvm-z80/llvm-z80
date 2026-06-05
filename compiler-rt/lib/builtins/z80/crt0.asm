; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
;===-- crt0.asm - Z80 C Runtime Startup ------------------------------------===;
;
; Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
; See https://llvm.org/LICENSE.txt for license information.
; SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
;
;===------------------------------------------------------------------------===;
;
; C runtime startup for Z80 (ELF toolchain).
; Linked first so it resides at address 0x0000 (reset vector).
; Sets up the stack pointer, zeroes .bss, and calls main().
;
; __bss_start and __bss_size are provided by the linker script (z80.ld).
;
;===------------------------------------------------------------------------===;

	.area _CODE
	.globl _start
	.globl _main
	.globl _halt

_start:
	ld	sp,#0		; SP = 0 wraps to 0xFFFE (top of 64KB RAM)

	;; Zero-fill .bss using LDIR block copy.
	;; Pattern: zero first byte, then LDIR propagates it forward.
	ld	hl,#__bss_start
	ld	bc,#__bss_size
	ld	a,b
	or	a,c
	jr	z,_bss_done	; skip if .bss is empty
	ld	(hl),#0		; zero first byte
	dec	bc
	ld	a,b
	or	a,c
	jr	z,_bss_done	; size was 1, already done
	ld	d,h
	ld	e,l
	inc	de		; DE = __bss_start + 1
	ldir			; copy BC bytes: (HL) -> (DE)
_bss_done:

	call	_main
_halt:
	halt
