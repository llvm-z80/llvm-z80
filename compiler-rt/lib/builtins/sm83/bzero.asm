; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _bzero

;===------------------------------------------------------------------------===;
; _bzero - Zero out memory block
;
; Input:  DE = ptr, BC = size
; Output: BC = ptr
; Uses E as zero holder for efficient loop.
;===------------------------------------------------------------------------===;

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
