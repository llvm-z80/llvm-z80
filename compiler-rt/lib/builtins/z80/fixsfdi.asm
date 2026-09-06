; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___fixsfdi

;===------------------------------------------------------------------------===;
; ___fixsfdi - Convert float to signed int64
;
; Calling convention (sret demotion, SDCC __sdcccall(1)):
;   Input:  HLDE = float (H=sign+exp_hi, L=exp_lo+mant_hi, D=mant_mid, E=mant_lo)
;   Stack:  SP+0,1 = return address, SP+2,3 = sret pointer (caller cleans)
;   Output: 8 bytes at *sret, little-endian
;
; The int128 routine at eight bytes: the mantissa is pre-shifted by the low
; three bits of the shift and the four bytes are then placed at the byte the
; rest of it names. Anything at 2^63 or above saturates instead.
;===------------------------------------------------------------------------===;
___fixsfdi:
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
	inc	a
	jr	nz, __fdi_finite
	ld	a, l
	and	#0x7F
	or	d
	or	e
	jr	z, __fdi_saturate
	jr	__fdi_zero

__fdi_finite:
	ld	a, b
	cp	#127
	jr	c, __fdi_zero
	cp	#190		; |value| >= 2^63
	jr	nc, __fdi_saturate

	set	7, l		; 24-bit mantissa in L:D:E

	ld	a, b
	sub	#150
	jr	c, __fdi_rshift

	; --- Left shift path: shift in 0..39 ---
	ld	c, a
	ld	b, #0
	and	#7
	jr	z, __fdi_lsh_done
__fdi_lsh_lp:
	sla	e
	rl	d
	rl	l
	rl	b
	dec	a
	jr	nz, __fdi_lsh_lp
__fdi_lsh_done:
	ld	a, c
	rrca
	rrca
	rrca
	and	#0x1F		; byte offset, 0..4
	push	bc
	ld	c, a
	ld	b, #0
	add	iy, bc
	pop	bc
	ld	0(iy), e
	ld	1(iy), d
	ld	2(iy), l
	ld	3(iy), b
	jr	__fdi_sign

	; --- Right shift path: shift in -23..-1 ---
__fdi_rshift:
	neg
__fdi_rsh_lp:
	srl	l
	rr	d
	rr	e
	dec	a
	jr	nz, __fdi_rsh_lp
	ld	0(iy), e
	ld	1(iy), d
	ld	2(iy), l

__fdi_sign:
	pop	af
	or	a
	ret	z
	; The result pointer, refetched: the shift above moved IY off the base
	; and the negation has to start there.
	pop	bc		; BC = return address
	pop	hl		; HL = sret pointer
	push	hl
	push	bc
	ld	b, #8
	or	a		; clear carry
__fdi_neg_lp:
	ld	a, #0
	sbc	a, (hl)
	ld	(hl), a
	inc	hl
	djnz	__fdi_neg_lp
	ret

__fdi_zero:
	pop	af
	ret

	; --- Clamp: INT64_MAX for +, INT64_MIN for - ---
__fdi_saturate:
	pop	af
	or	a
	jr	nz, __fdi_sat_neg
	push	iy
	pop	hl
	ld	b, #7
__fdi_sat_lp:
	ld	(hl), #0xFF
	inc	hl
	djnz	__fdi_sat_lp
	ld	(hl), #0x7F
	ret
__fdi_sat_neg:
	ld	a, #0x80
	ld	7(iy), a
	ret
