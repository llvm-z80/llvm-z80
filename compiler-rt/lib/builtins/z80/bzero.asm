; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _bzero

;===------------------------------------------------------------------------===;
; _bzero - Zero out memory block
;
; Input:  HL = ptr, DE = size
; Output: DE = ptr
;===------------------------------------------------------------------------===;

_bzero:
	push	hl		; save ptr
	ld	b, d		; BC = size
	ld	c, e
	ld	a, b
	or	c
	jr	z, _bzero_done
	ld	(hl), #0	; zero first byte
	dec	bc
	ld	a, b
	or	c
	jr	z, _bzero_done
	ld	d, h		; DE = HL (first byte)
	ld	e, l
	inc	de		; DE = HL + 1
	ldir			; propagate zero
_bzero_done:
	pop	de		; DE = original ptr
	ret
