; Test harness startup for Z80. Not the shipped crt0: this one exists so the
; runner can read a test's result out of a RAM dump instead of single stepping
; the whole program.
;
; z88dk-ticks only reports registers under -trace, which prints every executed
; instruction and slows emulation by more than two orders of magnitude; whole
; test suites time out under it. Recording main's return value at a known
; address lets the runner use -output, which dumps RAM at exit, and drop -trace
; entirely.
;
; Otherwise identical to compiler-rt/lib/builtins/z80/crt0.asm: SP at the top of
; RAM, .bss zeroed, then main. __bss_start and __bss_size come from z80.ld.

	.area _CODE
	.globl _start
	.globl _main
	.globl _halt
	.globl _exitcode

_start:
	ld	sp,#0		; SP = 0 wraps to 0xFFFE (top of 64KB RAM)

	;; Zero-fill .bss using LDIR block copy.
	ld	hl,#__bss_start
	ld	bc,#__bss_size
	ld	a,b
	or	a,c
	jr	z,_bss_done
	ld	(hl),#0
	dec	bc
	ld	a,b
	or	a,c
	jr	z,_bss_done
	ld	d,h
	ld	e,l
	inc	de
	ldir
_bss_done:

	call	_main
	ld	(_exitcode),de	; main returns i16 in DE
_halt:
	halt

	.area _BSS
_exitcode:
	.ds 2
