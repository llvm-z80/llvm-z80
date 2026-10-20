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
; Sets up the stack pointer, zeroes .bss and _DATA, copies _INITIALIZER to
; _INITIALIZED, and calls main().
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

	;; Zero fill _DATA
	ld	hl,#s__DATA
	ld	bc,#l__DATA
	ld	a,b
	or	a,c
	jr	z,_data_done	; skip if _DATA is empty
	ld	(hl),#0			; zero first byte
	dec	bc
	ld	a,b
	or	a,c
	jr	z,_data_done	; size was 1, already done
	ld	d,h
	ld	e,l
	inc	de				; DE = s__DATA + 1
	ldir				; copy BC bytes: (HL) -> (DE)
_data_done:

	;; Copy the _INITIALIZER section to the start of the _INITIALIZED section.
	;; It is expected that the _INITIALIZER section has the same size as the _INITIALIZED section.
	;; Otherwise, we have a layout error.
	ld	bc,#l__INITIALIZER
	ld	a,b
	or	a,c
	jr	z,_init_done 		; skip if _INITIALIZER is empty
	ld	hl,#s__INITIALIZER
	ld	de,#s__INITIALIZED
	ldir 					; copy _INITIALIZER to _INITIALIZED
_init_done:

	call	_main
_halt:
	halt

	;; Declare _BSS area so sdldz80 generates s__BSS and l__BSS symbols.
	;; Same thing for _DATA, _INITIALIZER and _INITIALIZED.
	.area _INITIALIZER
	.area _DATA
	.area _INITIALIZED
	.area _BSS
