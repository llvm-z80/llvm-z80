; Test harness startup for Z80 on the SDCC toolchain. Not the shipped crt0:
; this one records main's return value at _exitcode so the runner can read a
; test's result out of a RAM dump instead of single stepping the program with
; z88dk-ticks -trace, which slows emulation by more than two orders of
; magnitude.
;
; Otherwise identical to compiler-rt/lib/builtins/z80/crt0_sdcc.asm.

	.area _CODE
	.globl _start
	.globl _main
	.globl _halt
	.globl _exitcode

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
	ld	(_exitcode),de	; main returns i16 in DE
_halt:
	halt

	;; Declare _BSS area so sdldz80 generates s__BSS and l__BSS symbols.
	.area _BSS
_exitcode:
	.ds 2
