; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
;===-- crt0_sdcc.asm - SM83 C Runtime Startup (SDCC) -----------------------===;
;
; Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
; See https://llvm.org/LICENSE.txt for license information.
; SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
;
;===------------------------------------------------------------------------===;
;
; C runtime startup for SM83 (SDCC toolchain).
; Sets up the stack pointer, zeroes .bss, and calls main().
;
; s__BSS and l__BSS are provided automatically by the SDCC linker (sdldgb).
; The _halt symbol marks the end-of-execution address for emulators.
;
;===------------------------------------------------------------------------===;

	.area _CODE
	.globl _start
	.globl _main
	.globl _halt

_start:
	ld	sp,#0xFFFE	; top of WRAM (Game Boy: 0xC000-0xDFFF)

	;; Zero-fill .bss using ld (hl+),a auto-increment store.
	ld	hl,#s__BSS
	ld	de,#l__BSS
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

	;; Declare _BSS area so sdldgb generates s__BSS and l__BSS symbols.
	.area _BSS
