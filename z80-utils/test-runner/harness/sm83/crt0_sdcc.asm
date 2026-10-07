; Test harness startup for SM83 on the SDCC toolchain. Not the shipped crt0:
; this one records main's return value at _exitcode so the runner can read a
; test's result out of a RAM dump instead of single stepping the program with
; z88dk-ticks -trace, which slows emulation by more than two orders of
; magnitude.
;
; Otherwise identical to compiler-rt/lib/builtins/sm83/crt0_sdcc.asm.

	.area _CODE
	.globl _start
	.globl _main
	.globl _halt
	.globl _exitcode

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

	;; Copy the _INITIALIZER section to the start of the _INITIALIZED section.
	;; It is expected that the _INITIALIZER section has the same size as the _INITIALIZED section.
	;; Otherwise, we have a layout error.
	ld	bc,#l__INITIALIZER
	ld	a,b
	or	a,c
	jr	z,_init_done 		; skip if _INITIALIZER is empty
	ld	hl,#s__INITIALIZER
	ld	de,#s__INITIALIZED
_init_loop:
	ld	a,(hl+)				; a = (HL); HL++
	ld	(de),a
	inc de
	dec	bc
	ld	a,b
	or	a,c
	jr	nz,_init_loop
_init_done:

	;; main() has the hosted signature and the tests read argc: several put it
	;; in an array or add to it, so leaving whatever the .bss loop left in the
	;; argument register makes their result depend on where .bss ends. The
	;; suite runs each test once with no arguments, which is argc = 1.
	ld	de,#1		; argc
	ld	bc,#0		; argv

	call	_main
	;; main returns i16 in BC; SM83 has no `ld (nn),rr` for BC.
	ld	hl,#_exitcode
	ld	a,c
	ld	(hl+),a
	ld	a,b
	ld	(hl),a
_halt:
	halt

	;; Declare _BSS area so sdldgb generates s__BSS and l__BSS symbols.
	;; Same thing for _INITIALIZER and _INITIALIZED.
	.area _INITIALIZER
	.area _INITIALIZED
	.area _BSS
_exitcode:
	.ds 2
