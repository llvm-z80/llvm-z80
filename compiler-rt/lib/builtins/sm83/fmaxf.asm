; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _fmaxf

;===------------------------------------------------------------------------===;
; _fmaxf - Return maximum of two floats (SM83)
;
; Input:  DEBC = a, stack = b  (callee-cleanup for b)
; Output: DEBC = fmax(a, b)
;===------------------------------------------------------------------------===;
_fmaxf:
	push	de
	push	bc
	; SP+0..1: a_C,a_B  SP+2..3: a_E,a_D
	; SP+4..5: ret_addr  SP+6..9: b (C,B,E,D)

	; NaN check a
	ld	a, d
	and	#0x7F
	cp	#0x7F
	jr	nz, __fmaxf_a_ok
	bit	7, e
	jr	z, __fmaxf_a_ok
	ld	a, e
	and	#0x7F
	or	b
	or	c
	jr	nz, __fmaxf_ret_b
__fmaxf_a_ok:

	; NaN check b
	ld	hl, #9
	add	hl, sp
	ld	a, (hl)
	and	#0x7F
	cp	#0x7F
	jr	nz, __fmaxf_cmp
	dec	hl
	bit	7, (hl)
	jr	z, __fmaxf_cmp
	ld	a, (hl)
	and	#0x7F
	dec	hl
	or	(hl)
	dec	hl
	or	(hl)
	jr	nz, __fmaxf_ret_a

__fmaxf_cmp:
	; Push b for cmpsf2
	ld	hl, #8
	add	hl, sp
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, a
	push	hl
	ld	hl, #8
	add	hl, sp
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, a
	push	hl

	; Reload a
	ld	hl, #4
	add	hl, sp
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	inc	hl
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	call	___cmpsf2	; callee-cleanup, BC = result
	bit	7, b
	jr	z, __fmaxf_ret_a	; a >= b

__fmaxf_ret_b:
	ld	hl, #6
	add	hl, sp
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	inc	hl
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	jr	__fmaxf_done

__fmaxf_ret_a:
	pop	bc
	pop	de
	pop	hl
	inc	sp
	inc	sp
	inc	sp
	inc	sp
	jp	(hl)

__fmaxf_done:
	pop	hl
	pop	hl
	pop	hl
	inc	sp
	inc	sp
	inc	sp
	inc	sp
	jp	(hl)
