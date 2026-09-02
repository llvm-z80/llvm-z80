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
	.area _BSS
_exitcode:
	.ds 2
