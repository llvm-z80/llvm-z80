; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___fixunssfdi

;===------------------------------------------------------------------------===;
; ___fixunssfdi - Convert float to unsigned int64
;
; Calling convention (sret demotion, SDCC __sdcccall(1)):
;   Input:  HLDE = float (H=sign+exp_hi, L=exp_lo+mant_hi, D=mant_mid, E=mant_lo)
;   Stack:  SP+0,1 = return address, SP+2,3 = sret pointer (caller cleans)
;   Output: 8 bytes at *sret, little-endian
;
; The signed sibling without the negation: below zero and NaN answer zero, and
; 2^64 and above saturates.
;===------------------------------------------------------------------------===;
___fixunssfdi:
	pop	bc		; BC = return address
	pop	iy		; IY = sret pointer
	push	iy
	push	bc

	xor	a
	ld	0(iy), a
	ld	1(iy), a
	ld	2(iy), a
	ld	3(iy), a
	ld	4(iy), a
	ld	5(iy), a
	ld	6(iy), a
	ld	7(iy), a

	bit	7, h
	ret	nz		; negative -> 0

	ld	a, h
	add	a, a
	ld	b, a
	ld	a, l
	rlca
	and	#1
	or	b
	ld	b, a		; B = exponent

	inc	a
	jr	nz, __ufdi_finite
	ld	a, l
	and	#0x7F
	or	d
	or	e
	ret	nz		; NaN -> 0
	jr	__ufdi_saturate

__ufdi_finite:
	ld	a, b
	cp	#127
	ret	c
	cp	#191		; value >= 2^64
	jr	nc, __ufdi_saturate

	set	7, l

	ld	a, b
	sub	#150
	jr	c, __ufdi_rshift

	; --- Left shift path: shift in 0..40 ---
	ld	c, a
	ld	b, #0
	and	#7
	jr	z, __ufdi_lsh_done
__ufdi_lsh_lp:
	sla	e
	rl	d
	rl	l
	rl	b
	dec	a
	jr	nz, __ufdi_lsh_lp
__ufdi_lsh_done:
	ld	a, c
	rrca
	rrca
	rrca
	and	#0x1F		; byte offset, 0..5
	ld	c, a
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
	cp	#5
	ret	nc
	ld	3(iy), b
	ret

	; --- Right shift path: shift in -23..-1 ---
__ufdi_rshift:
	neg
__ufdi_rsh_lp:
	srl	l
	rr	d
	rr	e
	dec	a
	jr	nz, __ufdi_rsh_lp
	ld	0(iy), e
	ld	1(iy), d
	ld	2(iy), l
	ret

__ufdi_saturate:
	push	iy
	pop	hl
	ld	b, #8
__ufdi_sat_lp:
	ld	(hl), #0xFF
	inc	hl
	djnz	__ufdi_sat_lp
	ret
