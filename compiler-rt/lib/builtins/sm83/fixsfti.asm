; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___fixsfti

;===------------------------------------------------------------------------===;
; ___fixsfti - Convert float to signed int128
;
; Calling convention (sret demotion, SDCC __sdcccall(1)):
;   Input:  DEBC = float (D=sign+exp_hi, E=exp_lo+mant_hi, B=mant_mid, C=mant_lo)
;   Stack:  SP+0,1 = return address, SP+2,3 = sret pointer
;   Output: 16 bytes at *sret, little-endian; callee pops the sret slot
;
; Same algorithm as the Z80 version; the result pointer is refetched from
; the stack where needed since there is no spare index register.
;===------------------------------------------------------------------------===;
___fixsfti:
	; Zero the result
	ldhl	sp, #2
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a		; HL = sret pointer
	xor	a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a

	; Save sign
	ld	a, d
	and	#0x80
	push	af

	; Extract exponent into D (sign already saved)
	ld	a, d
	add	a, a
	ld	d, a
	ld	a, e
	rlca
	and	#1
	or	d
	ld	d, a		; D = exponent

	; NaN or infinity: exp = 255
	inc	a		; A = 0 iff exp = 255
	jr	nz, __fti_finite
	ld	a, e
	and	#0x7F
	or	b
	or	c
	jr	z, __fti_saturate	; infinity -> clamp
	jr	__fti_zero		; NaN -> 0
__fti_finite:
	; |value| < 1.0 -> 0
	ld	a, d
	cp	#127
	jr	c, __fti_zero
	; |value| >= 2^127 -> clamp
	cp	#254
	jr	nc, __fti_saturate

	; 24-bit mantissa with implicit bit: E:B:C (E = high)
	set	7, e

	; shift = exp - 150
	ld	a, d
	sub	#150
	jr	c, __fti_rshift

	; --- Left shift path: shift in 0..103 ---
	push	af		; save shift
	ld	d, #0		; D = fourth (top) byte
	and	#7
	jr	z, __fti_lsh_done
	ld	l, a
__fti_lsh_lp:
	sla	c
	rl	b
	rl	e
	rl	d
	dec	l
	jr	nz, __fti_lsh_lp
__fti_lsh_done:
	; byte offset = shift >> 3
	pop	af
	rrca
	rrca
	rrca
	and	#0x1F
	push	af
	; HL = sret + offset  (stack: offset, sign, ret, sret -> sret at SP+6)
	ldhl	sp, #6
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a
	pop	af
	add	a, l
	ld	l, a
	ld	a, #0
	adc	a, h
	ld	h, a
	ld	a, c
	ld	(hl+), a
	ld	a, b
	ld	(hl+), a
	ld	a, e
	ld	(hl+), a
	ld	a, d
	ld	(hl+), a
	jr	__fti_sign

	; --- Right shift path: shift in -23..-1 ---
__fti_rshift:
	cpl
	inc	a		; A = 1..23
__fti_rsh_lp:
	srl	e
	rr	b
	rr	c
	dec	a
	jr	nz, __fti_rsh_lp
	ldhl	sp, #4		; stack: sign, ret, sret
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a
	ld	a, c
	ld	(hl+), a
	ld	a, b
	ld	(hl+), a
	ld	a, e
	ld	(hl+), a

	; --- Apply sign ---
__fti_sign:
	pop	af
	or	a
	jr	z, __fti_done
	; Negate the 16-byte result in place
	ldhl	sp, #2
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a
	ld	d, #16
	or	a		; clear carry
__fti_neg_lp:
	ld	a, #0
	sbc	a, (hl)
	ld	(hl+), a
	dec	d
	jr	nz, __fti_neg_lp
	jr	__fti_done

__fti_zero:
	pop	af
__fti_done:
	pop	hl		; return address
	add	sp, #2		; callee-cleanup: skip sret slot
	jp	(hl)

	; --- Clamp: INT128_MAX for +, INT128_MIN for - ---
__fti_saturate:
	pop	af
	push	af
	ldhl	sp, #4
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a
	pop	af
	or	a
	jr	nz, __fti_sat_neg
	ld	d, #15
__fti_sat_lp:
	ld	a, #0xFF
	ld	(hl+), a
	dec	d
	jr	nz, __fti_sat_lp
	ld	(hl), #0x7F
	jr	__fti_done
__fti_sat_neg:
	; result is already zero; only the top byte differs
	ld	a, l
	add	a, #15
	ld	l, a
	ld	a, h
	adc	a, #0
	ld	h, a
	ld	(hl), #0x80
	jr	__fti_done
