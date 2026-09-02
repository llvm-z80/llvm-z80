; Test harness startup for SM83. Not the shipped crt0: this one exists so the
; runner can read a test's result out of a RAM dump instead of single stepping
; the whole program.
;
; z88dk-ticks only reports registers under -trace, which prints every executed
; instruction and slows emulation by more than two orders of magnitude; whole
; test suites time out under it. Recording main's return value at a known
; address lets the runner use -output, which dumps RAM at exit, and drop -trace
; entirely.
;
; Otherwise identical to compiler-rt/lib/builtins/sm83/crt0.asm: SP at the top
; of WRAM, .bss zeroed, then main. __bss_start and __bss_size come from sm83.ld.

	.area _CODE
	.globl _start
	.globl _main
	.globl _halt
	.globl _exitcode

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
	;; main returns i16 in BC. SM83 has no `ld (nn),rr` for BC, so store it
	;; a byte at a time through HL.
	ld	hl,#_exitcode
	ld	a,c
	ld	(hl+),a
	ld	a,b
	ld	(hl),a
_halt:
	halt

	.area _BSS
_exitcode:
	.ds 2
