; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _modff

;===------------------------------------------------------------------------===;
; _modff - Split a float into its integral and fractional parts (SM83)
;
; Input:  DEBC = x, stack SP+2,3 = float *iptr (callee-cleanup)
; Output: DEBC = fractional part, *iptr = integral part
;
; Same shape as the Z80 version: the integral part is a truncation toward zero
; and the fraction is what the subtraction leaves. An infinity and a zero are
; handed back unchanged for both parts, since a difference would turn the
; first into a NaN and lose the sign of the second.
;===------------------------------------------------------------------------===;
_modff:
	; Exponent into A.
	ld	a, d
	add	a, a
	ld	l, a
	ld	a, e
	rlca
	and	#1
	or	l		; A = exponent

	; An exponent of 255 with an empty mantissa is an infinity.
	inc	a
	jr	nz, __modff_finite
	ld	a, e
	and	#0x7F
	or	b
	or	c
	jr	z, __modff_whole
	jr	__modff_split	; a NaN: both parts come out of the split as NaN

__modff_finite:
	; A zero is an empty mantissa at exponent 0.
	dec	a
	or	a
	jr	nz, __modff_split
	ld	a, e
	and	#0x7F
	or	b
	or	c
	jr	nz, __modff_split

__modff_whole:
	ldhl	sp, #2
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a		; HL = iptr
	call	__modff_store	; *iptr = x
	ld	a, d
	and	#0x80		; keep the sign
	ld	d, a
	ld	e, #0
	ld	b, #0
	ld	c, #0
	jr	__modff_ret

__modff_split:
	; Keep x, since the subtraction needs it after truncf has overwritten it.
	push	de
	push	bc		; SP+0..3: x as C,B,E,D   SP+4,5: ret   SP+6,7: iptr

	call	_truncf		; DEBC = truncf(x)

	ldhl	sp, #6
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a		; HL = iptr
	call	__modff_store	; *iptr = truncf(x)

	; x - truncf(x), with the integral part as the second argument.
	push	de
	push	bc
	ldhl	sp, #4
	ld	a, (hl+)
	ld	c, a
	ld	a, (hl+)
	ld	b, a
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)		; DEBC = x
	call	___subsf3	; callee-cleanup, DEBC = x - truncf(x)

	; Subtracting equal values gives +0 whatever their sign, but the fraction
	; carries the sign of x, so an integral negative needs it put back. The
	; saved x is still below, so its sign byte is at SP+3.
	ld	a, d
	and	#0x7F
	or	e
	or	b
	or	c
	jr	nz, __modff_drop
	ldhl	sp, #3
	ld	a, (hl)
	and	#0x80
	ld	d, a
	ld	e, #0
	ld	b, #0
	ld	c, #0

__modff_drop:
	add	sp, #4		; drop the saved x

__modff_ret:
	; Callee-cleanup: step over iptr on the way out.
	pop	hl		; return address
	add	sp, #2
	jp	(hl)

; Write DEBC to (HL), little-endian. Clobbers A and HL.
__modff_store:
	ld	a, c
	ld	(hl+), a
	ld	a, b
	ld	(hl+), a
	ld	a, e
	ld	(hl+), a
	ld	a, d
	ld	(hl), a
	ret
