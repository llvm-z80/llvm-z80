; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _ceilf

;===------------------------------------------------------------------------===;
; _ceilf - Ceiling: round toward positive infinity
;
; Input:  DEBC = float
; Output: DEBC = ceil(x)
;===------------------------------------------------------------------------===;
_ceilf:
	ld	a, d
	add	a, a
	ld	h, a
	ld	a, e
	rlca
	and	#1
	or	h

	cp	#150
	ret	nc

	cp	#127
	jr	c, __ceilf_small

	push	de
	push	bc

	sub	#127
	cp	#16
	jr	nc, __ceilf_hi
	cp	#8
	jr	nc, __ceilf_mid

	ld	h, a
	ld	a, #7
	sub	h
	jr	z, __ceilf_l_done
	ld	h, a
	ld	a, #0xFF
__ceilf_l_lp:
	sla	a
	dec	h
	jr	nz, __ceilf_l_lp
	and	e
	ld	e, a
__ceilf_l_done:
	ld	b, #0
	ld	c, #0
	jr	__ceilf_check

__ceilf_mid:
	; 8 <= A < 16: the low byte is entirely fractional, and the middle byte
	; keeps A - 7 bits, the top mantissa byte holding seven already.
	sub	#7		; A = integer bits in the middle byte (1..8)
	ld	h, a
	ld	a, #8
	sub	h		; A = bits to clear (7..0)
	ld	c, #0
	jr	z, __ceilf_check
	ld	h, a
	ld	a, #0xFF
__ceilf_m_lp:
	sla	a
	dec	h
	jr	nz, __ceilf_m_lp
	and	b
	ld	b, a
	jr	__ceilf_check

__ceilf_hi:
	; 16 <= A < 23: only the low byte still has fractional bits, and it
	; keeps A - 15 of them.
	sub	#15		; A = integer bits in the low byte (1..7)
	ld	h, a
	ld	a, #8
	sub	h		; A = bits to clear (7..1)
	jr	z, __ceilf_check
	ld	h, a
	ld	a, #0xFF
__ceilf_h_lp:
	sla	a
	dec	h
	jr	nz, __ceilf_h_lp
	and	c
	ld	c, a

__ceilf_check:
	bit	7, d
	jr	nz, __ceilf_done	; negative → trunc == ceil

	pop	hl		; HL = original BC
	ld	a, c
	xor	l
	jr	nz, __ceilf_add1
	ld	a, b
	xor	h
	jr	nz, __ceilf_add1
	pop	hl		; HL = original DE
	ld	a, e
	xor	l
	jr	nz, __ceilf_add1b
	ld	a, d
	xor	h
	jr	nz, __ceilf_add1b
	ret

__ceilf_add1:
	pop	hl
__ceilf_add1b:
	; trunc(x) + 1.0
	ld	hl, #0x3F80
	push	hl
	ld	hl, #0x0000
	push	hl
	call	___addsf3	; callee-cleanup
	ret

__ceilf_done:
	pop	hl
	pop	hl
	ret

__ceilf_small:
	bit	7, d
	jr	nz, __ceilf_neg0
	ld	a, d
	and	#0x7F
	or	e
	or	b
	or	c
	jr	z, __ceilf_pzero
	ld	de, #0x3F80	; 1.0f
	ld	bc, #0x0000
	ret
__ceilf_pzero:
	ret
__ceilf_neg0:
	ld	d, #0x80
	ld	e, #0
	ld	b, e
	ld	c, e
	ret
