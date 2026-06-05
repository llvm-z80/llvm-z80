; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
;===-- crt0_sdcc.asm - Z80 C Runtime Startup (SDCC) ------------------------===;
;
; Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
; See https://llvm.org/LICENSE.txt for license information.
; SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
;
;===------------------------------------------------------------------------===;
;
; C runtime startup for Z80 (SDCC toolchain).
; Sets up the stack pointer, zeroes .bss, and calls main().
;
; s__BSS and l__BSS are provided automatically by the SDCC linker (sdldz80).
; The _halt symbol marks the end-of-execution address for emulators.
;
;===------------------------------------------------------------------------===;

	.area _CODE
	.globl _start
	.globl _main
	.globl _halt

_start:
	ld	sp,#0		; SP = 0 wraps to 0xFFFE (top of 64KB RAM)

	;; Zero-fill .bss using LDIR block copy.
	ld	hl,#s__BSS
	ld	bc,#l__BSS
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
	inc	de		; DE = s__BSS + 1
	ldir			; copy BC bytes: (HL) -> (DE)
_bss_done:

	call	_main
_halt:
	halt

	;; Declare _BSS area so sdldz80 generates s__BSS and l__BSS symbols.
	.area _BSS
