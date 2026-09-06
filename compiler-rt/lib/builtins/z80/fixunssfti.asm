; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___fixunssfti

;===------------------------------------------------------------------------===;
; ___fixunssfti - Convert float to unsigned int128
;
; Calling convention (sret demotion, SDCC __sdcccall(1)):
;   Input:  HLDE = float (H=sign+exp_hi, L=exp_lo+mant_hi, D=mant_mid, E=mant_lo)
;   Stack:  SP+0,1 = return address, SP+2,3 = sret pointer (caller cleans)
;   Output: 16 bytes at *sret, little-endian
;
; The signed sibling's algorithm without the negation: anything below zero,
; and a NaN, answer zero, and only an infinity saturates. A finite float is
; always in range, since the largest one is under 2^128.
;===------------------------------------------------------------------------===;
___fixunssfti:
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

	; Anything negative has no unsigned answer; zero is the one this gives.
	bit	7, h
	ret	nz

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
	jr	nz, __ufti_finite
	ld	a, l
	and	#0x7F
	or	d
	or	e
	ret	nz		; NaN -> 0
	jr	__ufti_saturate	; +infinity -> UINT128_MAX

__ufti_finite:
	; |value| < 1.0 -> 0
	ld	a, b
	cp	#127
	ret	c

	; 24-bit mantissa with implicit bit: L:D:E (L = high)
	set	7, l

	; shift = exp - 150
	ld	a, b
	sub	#150
	jr	c, __ufti_rshift

	; --- Left shift path: shift in 0..104 ---
	ld	c, a		; C = shift
	ld	b, #0		; B = fourth (top) byte
	and	#7
	jr	z, __ufti_lsh_done
__ufti_lsh_lp:
	sla	e
	rl	d
	rl	l
	rl	b
	dec	a
	jr	nz, __ufti_lsh_lp
__ufti_lsh_done:
	; IY += shift >> 3
	ld	a, c
	rrca
	rrca
	rrca
	and	#0x1F
	ld	c, a		; C = byte offset, 0..13
	push	bc
	ld	b, #0
	add	iy, bc
	pop	bc
	ld	0(iy), e
	ld	1(iy), d
	ld	2(iy), l
	; The top byte lands past the end only at the largest exponent, where the
	; shift is a whole number of bytes and so leaves that byte empty anyway.
	ld	a, c
	cp	#13
	ret	nc
	ld	3(iy), b
	ret

	; --- Right shift path: shift in -23..-1 ---
__ufti_rshift:
	neg			; A = 1..23
__ufti_rsh_lp:
	srl	l
	rr	d
	rr	e
	dec	a
	jr	nz, __ufti_rsh_lp
	ld	0(iy), e
	ld	1(iy), d
	ld	2(iy), l
	ret

__ufti_saturate:
	push	iy
	pop	hl
	ld	b, #16
__ufti_sat_lp:
	ld	(hl), #0xFF
	inc	hl
	djnz	__ufti_sat_lp
	ret
