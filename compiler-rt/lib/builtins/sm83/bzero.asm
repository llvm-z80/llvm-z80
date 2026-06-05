; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _bzero
	.globl _bzero_loop
	.globl _bzero_done

_bzero:
	push	de		; save ptr for return
	ld	h, d
	ld	l, e		; HL = ptr
	ld	a, b
	or	c
	jr	z, _bzero_done
	ld	e, #0		; E = 0 (fill value)
_bzero_loop:
	ld	(hl), e		; *HL = 0
	inc	hl
	dec	bc
	ld	a, b
	or	c
	jr	nz, _bzero_loop
_bzero_done:
	pop	bc		; BC = original ptr
	ret

;===------------------------------------------------------------------------===;
;=== 32-bit Integer Arithmetic ===============================================;
;===------------------------------------------------------------------------===;
;
; 32-bit division and modulo via restoring division algorithm.
; SM83 lacks shadow registers, IX/IY, DJNZ, SBC HL, ADC HL.
;
; Calling convention: __sdcccall(1) for SM83
;   First i32 argument in DEBC (DE=high, BC=low)
;   Second i32 argument on stack
;   Return i32 in DEBC
;   No callee-saved registers
