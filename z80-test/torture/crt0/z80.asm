; Startup code for the GCC torture tests on the Z80. main, abort() and exit()
; end at the same HALT with the result in DE:
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
	ld	sp,#0		; the first push wraps to the top of memory

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

	;; Several tests read argc, so main gets the argc of a run without
	;; arguments.
	ld	hl,#1		; argc
	ld	de,#0		; argv
	call	_main
_halt:
	halt

_abort:
	ld	de,#0xDEAD
	jr	_halt

_exit:
	ex	de,hl		; the status arrives in HL
	jr	_halt
