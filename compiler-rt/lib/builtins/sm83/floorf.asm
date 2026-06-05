; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _floorf

;===------------------------------------------------------------------------===;
; _floorf - Floor: round toward negative infinity
;
; Input:  DEBC = float
; Output: DEBC = floor(x)
;
; Algorithm: trunc(x), then subtract 1.0 if x < 0 and had fractional bits.
; __subsf3 convention: arg1=DEBC, arg2=stack (callee-cleanup), returns DEBC.
;===------------------------------------------------------------------------===;
_floorf:
	; Extract exponent
	ld	a, d
	add	a, a
	ld	h, a
	ld	a, e
	rlca
	and	#1
	or	h		; A = exponent

	cp	#150
	ret	nc		; already integer

	cp	#127
	jr	c, __floorf_small

	; 127 <= exp < 150: has fractional bits
	; Save original
	push	de
	push	bc

	; Truncate inline
	sub	#127
	cp	#16
	jr	nc, __floorf_hi
	cp	#8
	jr	nc, __floorf_mid

	ld	h, a
	ld	a, #7
	sub	h
	jr	z, __floorf_l_done
	ld	h, a
	ld	a, #0xFF
__floorf_l_lp:
	sla	a
	dec	h
	jr	nz, __floorf_l_lp
	and	e
	ld	e, a
__floorf_l_done:
	ld	b, #0
	ld	c, #0
	jr	__floorf_check

__floorf_mid:
	sub	#8
	jr	z, __floorf_mid_zero
	ld	h, a
	ld	a, #8
	sub	h
	ld	h, a
	ld	a, #0xFF
__floorf_m_lp:
	sla	a
	dec	h
	jr	nz, __floorf_m_lp
	and	b
	ld	b, a
	ld	c, #0
	jr	__floorf_check
__floorf_mid_zero:
	ld	c, #0
	jr	__floorf_check

__floorf_hi:
	sub	#16
	jr	z, __floorf_check
	ld	h, a
	ld	a, #8
	sub	h
	ld	h, a
	ld	a, #0xFF
__floorf_h_lp:
	sla	a
	dec	h
	jr	nz, __floorf_h_lp
	and	c
	ld	c, a

__floorf_check:
	; DEBC = trunc(x). Stack: original BC, original DE
	bit	7, d
	jr	z, __floorf_done	; positive → trunc == floor

	; Negative: check if trunc != original
	pop	hl		; HL = original BC
	ld	a, c
	xor	l
	jr	nz, __floorf_sub1
	ld	a, b
	xor	h
	jr	nz, __floorf_sub1
	pop	hl		; HL = original DE
	ld	a, e
	xor	l
	jr	nz, __floorf_sub1b
	ld	a, d
	xor	h
	jr	nz, __floorf_sub1b
	ret			; trunc == original

__floorf_sub1:
	pop	hl		; discard original DE
__floorf_sub1b:
	; trunc(x) - 1.0: arg1=DEBC=trunc(x), arg2=1.0 on stack
	; 1.0f = 0x3F800000 → D=0x3F, E=0x80, B=0x00, C=0x00
	ld	hl, #0x3F80	; H=D2=0x3F, L=E2=0x80
	push	hl
	ld	hl, #0x0000	; H=B2=0x00, L=C2=0x00
	push	hl
	call	___subsf3	; callee-cleanup
	ret

__floorf_done:
	pop	hl		; discard original BC
	pop	hl		; discard original DE
	ret

__floorf_small:
	bit	7, d
	jr	nz, __floorf_neg1
	ld	d, #0
	ld	e, d
	ld	b, d
	ld	c, d
	ret
__floorf_neg1:
	ld	a, d
	and	#0x7F
	or	e
	or	b
	or	c
	jr	z, __floorf_nzero
	ld	de, #0xBF80	; -1.0f
	ld	bc, #0x0000
	ret
__floorf_nzero:
	ld	d, #0x80
	ld	e, #0
	ld	b, e
	ld	c, e
	ret
