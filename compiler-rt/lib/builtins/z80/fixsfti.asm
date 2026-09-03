; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___fixsfti

;===------------------------------------------------------------------------===;
; ___fixsfti - Convert float to signed int128
;
; Calling convention (sret demotion, SDCC __sdcccall(1)):
;   Input:  HLDE = float (H=sign+exp_hi, L=exp_lo+mant_hi, D=mant_mid, E=mant_lo)
;   Stack:  SP+0,1 = return address, SP+2,3 = sret pointer (caller cleans)
;   Output: 16 bytes at *sret, little-endian
;
; Algorithm:
;   1. Extract sign, exponent, 24-bit mantissa with implicit bit
;   2. NaN -> 0; exp < 127 (|value| < 1.0) -> 0
;   3. exp >= 254 (|value| >= 2^127) -> clamp to INT128_MAX/MIN
;   4. shift = exp - 150: negative -> right shift mantissa in registers,
;      positive -> pre-shift by (shift & 7) in registers and place the four
;      bytes at byte offset (shift >> 3) of the zeroed result
;   5. Negate the 16-byte result if the sign was set
;===------------------------------------------------------------------------===;
___fixsfti:
	; Fetch the sret pointer without disturbing the stack
	pop	bc		; BC = return address
	pop	iy		; IY = sret pointer
	push	iy
	push	bc

	; Zero the result
	xor	a
	ld	0(iy), a
	ld	1(iy), a
	ld	2(iy), a
	ld	3(iy), a
	ld	4(iy), a
	ld	5(iy), a
	ld	6(iy), a
	ld	7(iy), a
	ld	8(iy), a
	ld	9(iy), a
	ld	10(iy), a
	ld	11(iy), a
	ld	12(iy), a
	ld	13(iy), a
	ld	14(iy), a
	ld	15(iy), a

	; Save sign
	ld	a, h
	and	#0x80
	push	af

	; Extract exponent into B
	ld	a, h
	add	a, a
	ld	b, a
	ld	a, l
	rlca
	and	#1
	or	b
	ld	b, a		; B = exponent

	; NaN or infinity: exp = 255
	inc	a		; A = 0 iff exp = 255
	jr	nz, __fti_finite
	ld	a, l
	and	#0x7F
	or	d
	or	e
	jr	z, __fti_saturate	; infinity -> clamp
	jr	__fti_zero		; NaN -> 0
__fti_finite:
	; |value| < 1.0 -> 0
	ld	a, b
	cp	#127
	jr	c, __fti_zero
	; |value| >= 2^127 -> clamp
	cp	#254
	jr	nc, __fti_saturate

	; 24-bit mantissa with implicit bit: L:D:E (L = high)
	set	7, l

	; shift = exp - 150
	ld	a, b
	sub	#150
	jr	c, __fti_rshift

	; --- Left shift path: shift in 0..103 ---
	ld	c, a		; C = shift
	ld	b, #0		; B = fourth (top) byte
	and	#7
	jr	z, __fti_lsh_done
__fti_lsh_lp:
	sla	e
	rl	d
	rl	l
	rl	b
	dec	a
	jr	nz, __fti_lsh_lp
__fti_lsh_done:
	; IY += shift >> 3
	ld	a, c
	rrca
	rrca
	rrca
	and	#0x1F
	push	bc
	ld	c, a
	ld	b, #0
	add	iy, bc
	pop	bc
	ld	0(iy), e
	ld	1(iy), d
	ld	2(iy), l
	ld	3(iy), b
	jr	__fti_sign

	; --- Right shift path: shift in -23..-1 ---
__fti_rshift:
	neg			; A = 1..23
__fti_rsh_lp:
	srl	l
	rr	d
	rr	e
	dec	a
	jr	nz, __fti_rsh_lp
	ld	0(iy), e
	ld	1(iy), d
	ld	2(iy), l

	; --- Apply sign ---
__fti_sign:
	pop	af
	or	a
	ret	z
	; Negate the 16-byte result in place
	push	iy
	pop	hl
	ld	b, #16
	or	a		; clear carry
__fti_neg_lp:
	ld	a, #0
	sbc	a, (hl)
	ld	(hl), a
	inc	hl
	djnz	__fti_neg_lp
	ret

__fti_zero:
	pop	af
	ret

	; --- Clamp: INT128_MAX for +, INT128_MIN for - ---
__fti_saturate:
	pop	af
	or	a
	jr	nz, __fti_sat_neg
	push	iy
	pop	hl
	ld	b, #15
__fti_sat_lp:
	ld	(hl), #0xFF
	inc	hl
	djnz	__fti_sat_lp
	ld	(hl), #0x7F
	ret
__fti_sat_neg:
	ld	a, #0x80
	ld	15(iy), a
	ret
