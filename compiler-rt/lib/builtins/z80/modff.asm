; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _modff

;===------------------------------------------------------------------------===;
; _modff - Split a float into its integral and fractional parts
;
; Input:  HLDE = x, stack SP+2,3 = float *iptr (callee-cleanup)
; Output: HLDE = fractional part, *iptr = integral part
;
; Both parts carry the sign of x, so the integral part is a truncation toward
; zero and the fraction is what is left over:
;
;   *iptr = truncf(x)
;   return x - *iptr
;
; The subtraction is wrong for the two inputs where the parts are not a
; difference: an infinity would give NaN rather than a zero, and a zero would
; lose its sign. Both are answered by handing x back unchanged for both parts.
;===------------------------------------------------------------------------===;
_modff:
	push	ix
	ld	ix, #0
	add	ix, sp
	; IX+0,1: saved IX   IX+2,3: return address   IX+4,5: iptr

	; Exponent into A, so the two cases that are not a difference can be seen.
	ld	a, h
	add	a, a
	ld	b, a
	ld	a, l
	rlca
	and	#1
	or	b		; A = exponent

	; An exponent of 255 with an empty mantissa is an infinity.
	inc	a
	jr	nz, __modff_finite
	ld	a, l
	and	#0x7F
	or	d
	or	e
	jr	z, __modff_whole
	jr	__modff_split	; a NaN: both parts come out of the split as NaN

__modff_finite:
	; A zero is an empty mantissa at exponent 0. Both halves are x itself,
	; and going through the subtraction would return +0 for -0.
	dec	a		; undo the test above
	or	a
	jr	nz, __modff_split
	ld	a, l
	and	#0x7F
	or	d
	or	e
	jr	nz, __modff_split

__modff_whole:
	; *iptr = x, and the other part is x with the magnitude taken out.
	call	__modff_store
	ld	a, h
	and	#0x80		; keep the sign
	ld	h, a
	ld	l, #0
	ld	d, #0
	ld	e, #0
	jr	__modff_ret

__modff_split:
	; Keep x below the frame, since truncf and the subtraction need it again.
	push	hl
	push	de		; IX-4..-1: x_E, x_D, x_L, x_H

	call	_truncf		; HLDE = truncf(x)
	call	__modff_store	; *iptr = truncf(x)

	; x - truncf(x), with the integral part as the second argument.
	push	hl
	push	de
	ld	h, -1(ix)
	ld	l, -2(ix)
	ld	d, -3(ix)
	ld	e, -4(ix)
	call	___subsf3	; callee-cleanup, HLDE = x - truncf(x)

	; Subtracting equal values gives +0 whatever their sign, but the fraction
	; carries the sign of x, so an integral negative needs it put back.
	ld	a, h
	and	#0x7F
	or	l
	or	d
	or	e
	jr	nz, __modff_ret
	ld	a, -1(ix)	; the sign byte of the saved x
	and	#0x80
	ld	h, a
	ld	l, #0
	ld	d, l
	ld	e, l

__modff_ret:
	ld	sp, ix
	pop	ix
	; Callee-cleanup: step over iptr on the way out.
	pop	bc		; return address
	inc	sp
	inc	sp
	push	bc
	ret

; Write HLDE to *iptr, little-endian, leaving HLDE alone.
__modff_store:
	ld	c, 4(ix)
	ld	b, 5(ix)
	ld	a, e
	ld	(bc), a
	inc	bc
	ld	a, d
	ld	(bc), a
	inc	bc
	ld	a, l
	ld	(bc), a
	inc	bc
	ld	a, h
	ld	(bc), a
	ret
