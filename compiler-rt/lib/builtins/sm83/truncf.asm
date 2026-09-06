; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _truncf

;===------------------------------------------------------------------------===;
; _truncf - Truncate float toward zero (remove fractional part)
;
; Input:  DEBC = float (D=MSB, C=LSB)
; Output: DEBC = truncated float
;
; SM83 IEEE 754: D[7]=sign, D[6:0]=exp[7:1], E[7]=exp[0],
;                E[6:0]:B:C = mantissa[22:0]
;===------------------------------------------------------------------------===;
_truncf:
	; Extract exponent
	ld	a, d
	add	a, a		; A = exp[7:1] << 1
	ld	h, a
	ld	a, e
	rlca
	and	#1
	or	h		; A = full exponent

	; exp < 127 → |x| < 1.0 → return ±0.0
	cp	#127
	jr	c, __truncf_zero

	; exp >= 150 → already integer
	cp	#150
	ret	nc

	; 127 <= exp < 150: mask off fractional bits
	sub	#127		; A = integer mantissa bits (0..22)

	cp	#16
	jr	nc, __truncf_hi
	cp	#8
	jr	nc, __truncf_mid

	; A < 8: clear part of E, all of B, C
	ld	h, a
	ld	a, #7
	sub	h
	jr	z, __truncf_l_done
	ld	h, a
	ld	a, #0xFF
__truncf_l_lp:
	sla	a
	dec	h
	jr	nz, __truncf_l_lp
	and	e
	ld	e, a
__truncf_l_done:
	ld	b, #0
	ld	c, #0
	ret

__truncf_mid:
	; 8 <= A < 16: C is entirely fractional, and B keeps its top bits.
	; E already holds seven integer bits, so B keeps A - 7 of them.
	sub	#7		; A = integer bits in B (1..8)
	ld	h, a
	ld	a, #8
	sub	h		; A = bits to clear in B (7..0)
	ld	c, #0
	jr	z, __truncf_mid_done
	ld	h, a
	ld	a, #0xFF
__truncf_m_lp:
	sla	a
	dec	h
	jr	nz, __truncf_m_lp
	and	b
	ld	b, a
__truncf_mid_done:
	ret

__truncf_hi:
	; 16 <= A < 23: E and B are entirely integer, and C keeps A - 15 bits.
	sub	#15		; A = integer bits in C (1..7)
	ld	h, a
	ld	a, #8
	sub	h		; A = bits to clear in C (7..1)
	ret	z
	ld	h, a
	ld	a, #0xFF
__truncf_e_lp:
	sla	a
	dec	h
	jr	nz, __truncf_e_lp
	and	c
	ld	c, a
	ret

__truncf_zero:
	ld	a, d
	and	#0x80
	ld	d, a
	ld	e, #0
	ld	b, e
	ld	c, e
	ret
