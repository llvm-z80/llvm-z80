; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
;===-- crt0.asm - SM83 (Game Boy) C Runtime Startup ------------------------===;
;
; Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
; See https://llvm.org/LICENSE.txt for license information.
; SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
;
;===------------------------------------------------------------------------===;
;
; C runtime startup for SM83 (ELF toolchain).
; Linked first so it resides at address 0x0000.
; Sets up the stack pointer, zeroes .bss, and calls main().
;
; __bss_start and __bss_size are provided by the linker script (sm83.ld).
;
;===------------------------------------------------------------------------===;

	.area _CODE
	.globl _start
	.globl _main
	.globl _halt

_start:
	ld	sp,#0xFFFE	; top of WRAM (Game Boy: 0xC000-0xDFFF)

	;; Zero-fill .bss using ld (hl+),a auto-increment store.
	ld	hl,#__bss_start
	ld	de,#__bss_size
	ld	a,d
	or	a,e
	jr	z,_bss_done	; skip if .bss is empty
	xor	a,a		; A = 0
_bss_loop:
	ld	(hl+),a		; (HL) = 0; HL++
	dec	de
	ld	a,d
	or	a,e
	ld	a,#0		; reset A without affecting flags
	jr	nz,_bss_loop
_bss_done:

	call	_main
_halt:
	halt
