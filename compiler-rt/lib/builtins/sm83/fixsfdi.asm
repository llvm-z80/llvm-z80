; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___fixsfdi

;===------------------------------------------------------------------------===;
; ___fixsfdi - Convert float to signed int64 (SM83)
;
; Calling convention (sret demotion, SDCC __sdcccall(1)):
;   Input:  DEBC = float (D=sign+exp_hi, E=exp_lo+mant_hi, B=mant_mid, C=mant_lo)
;   Stack:  SP+0,1 = return address, SP+2,3 = sret pointer
;   Output: 8 bytes at *sret, little-endian; callee pops the sret slot
;
; The int128 routine at eight bytes. The largest shift still leaves room for
; all four bytes, so unlike the unsigned form there is no tail for it.
;===------------------------------------------------------------------------===;
___fixsfdi:
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

	; Save sign
	ld	a, d
	and	#0x80
	push	af

	; Exponent into D
	ld	a, d
	add	a, a
	ld	d, a
	ld	a, e
	rlca
	and	#1
	or	d
	ld	d, a		; D = exponent

	inc	a
	jr	nz, __fdi_finite
	ld	a, e
	and	#0x7F
	or	b
	or	c
	jr	z, __fdi_saturate
	jr	__fdi_zero

__fdi_finite:
	ld	a, d
	cp	#127
	jr	c, __fdi_zero
	cp	#190		; |value| >= 2^63
	jr	nc, __fdi_saturate

	set	7, e		; 24-bit mantissa in E:B:C

	ld	a, d
	sub	#150
	jr	c, __fdi_rshift

	; --- Left shift path: shift in 0..39 ---
	push	af
	ld	d, #0
	and	#7
	jr	z, __fdi_lsh_done
	ld	l, a
__fdi_lsh_lp:
	sla	c
	rl	b
	rl	e
	rl	d
	dec	l
	jr	nz, __fdi_lsh_lp
__fdi_lsh_done:
	pop	af
	rrca
	rrca
	rrca
	and	#0x1F		; byte offset, 0..4
	push	af
	ldhl	sp, #6		; stack: offset, sign, ret, sret
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
	jr	__fdi_sign

	; --- Right shift path: shift in -23..-1 ---
__fdi_rshift:
	cpl
	inc	a		; A = 1..23
__fdi_rsh_lp:
	srl	e
	rr	b
	rr	c
	dec	a
	jr	nz, __fdi_rsh_lp
	ldhl	sp, #4		; stack: sign, ret, sret
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a
	ld	a, c
	ld	(hl+), a
	ld	a, b
	ld	(hl+), a
	ld	a, e
	ld	(hl), a

__fdi_sign:
	pop	af
	or	a
	jr	z, __fdi_done
	ldhl	sp, #2
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a
	ld	d, #8
	or	a		; clear carry
__fdi_neg_lp:
	ld	a, #0
	sbc	a, (hl)
	ld	(hl+), a
	dec	d
	jr	nz, __fdi_neg_lp
	jr	__fdi_done

__fdi_zero:
	pop	af
__fdi_done:
	pop	hl		; return address
	add	sp, #2		; callee-cleanup: skip the sret slot
	jp	(hl)

	; --- Clamp: INT64_MAX for +, INT64_MIN for - ---
__fdi_saturate:
	pop	af
	push	af
	ldhl	sp, #4
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a
	pop	af
	or	a
	jr	nz, __fdi_sat_neg
	ld	d, #7
__fdi_sat_lp:
	ld	a, #0xFF
	ld	(hl+), a
	dec	d
	jr	nz, __fdi_sat_lp
	ld	(hl), #0x7F
	jr	__fdi_done
__fdi_sat_neg:
	; the result is already zero; only the top byte differs
	ld	a, l
	add	a, #7
	ld	l, a
	ld	a, h
	adc	a, #0
	ld	h, a
	ld	(hl), #0x80
	jr	__fdi_done
