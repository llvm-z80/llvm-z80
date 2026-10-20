; Startup code for the GCC torture tests on the SM83. main, abort() and exit()
; end at the same HALT with the result in BC:
;
;   main() returns n  ->  n
;   abort()           ->  0xDEAD
;   exit(n)           ->  n
;
; link_error() and link_error0..7 are not defined, so a call the optimizer
; should have deleted fails to link.

	.area _CODE
	.globl _start
	.globl _main
	.globl _abort
	.globl _exit

_start:
	ld	sp,#0xFFFE

	ld	hl,#__bss_start
	ld	de,#__bss_size
	ld	a,d
	or	a,e
	jr	z,_bss_done
	xor	a,a
_bss_loop:
	ld	(hl+),a
	dec	de
	ld	a,d
	or	a,e
	ld	a,#0		; reset A without affecting flags
	jr	nz,_bss_loop
_bss_done:

	;; Several tests read argc, so main gets the argc of a run without
	;; arguments.
	ld	de,#1		; argc
	ld	bc,#0		; argv
	call	_main
_halt:
	halt

_abort:
	ld	bc,#0xDEAD
	jr	_halt

_exit:
	ld	b,d		; the status arrives in DE
	ld	c,e
	jr	_halt
