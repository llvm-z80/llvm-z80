; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___fixunssfdi

;===------------------------------------------------------------------------===;
; ___fixunssfdi - Convert float to unsigned int64 (SM83)
;
; Calling convention (sret demotion, SDCC __sdcccall(1)):
;   Input:  DEBC = float (D=sign+exp_hi, E=exp_lo+mant_hi, B=mant_mid, C=mant_lo)
;   Stack:  SP+0,1 = return address, SP+2,3 = sret pointer
;   Output: 8 bytes at *sret, little-endian; callee pops the sret slot
;
; The signed sibling without the negation, and with its own tail for the
; largest exponent, where the mantissa lands on the last three bytes.
;===------------------------------------------------------------------------===;
___fixunssfdi:
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

	bit	7, d
	jp	nz, __ufdi_done	; negative -> 0

	ld	a, d
	add	a, a
	ld	d, a
	ld	a, e
	rlca
	and	#1
	or	d
	ld	d, a		; D = exponent

	inc	a
	jr	nz, __ufdi_finite
	ld	a, e
	and	#0x7F
	or	b
	or	c
	jr	nz, __ufdi_done	; NaN -> 0
	jr	__ufdi_saturate

__ufdi_finite:
	ld	a, d
	cp	#127
	jr	c, __ufdi_done
	cp	#191		; value >= 2^64
	jr	nc, __ufdi_saturate

	set	7, e		; 24-bit mantissa in E:B:C

	ld	a, d
	cp	#190
	jr	z, __ufdi_top

	sub	#150
	jr	c, __ufdi_rshift

	; --- Left shift path: shift in 0..39 ---
	push	af
	ld	d, #0
	and	#7
	jr	z, __ufdi_lsh_done
	ld	l, a
__ufdi_lsh_lp:
	sla	c
	rl	b
	rl	e
	rl	d
	dec	l
	jr	nz, __ufdi_lsh_lp
__ufdi_lsh_done:
	pop	af
	rrca
	rrca
	rrca
	and	#0x1F		; byte offset
	push	af
	ldhl	sp, #4		; stack: offset, ret, sret
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
	jr	__ufdi_done

__ufdi_top:
	; exp 190: the mantissa moves a whole five bytes up and fills the last
	; three, leaving nothing for a fourth byte to carry.
	ldhl	sp, #2
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a
	ld	a, l
	add	a, #5
	ld	l, a
	ld	a, h
	adc	a, #0
	ld	h, a
	ld	a, c
	ld	(hl+), a
	ld	a, b
	ld	(hl+), a
	ld	a, e
	ld	(hl), a
	jr	__ufdi_done

	; --- Right shift path: shift in -23..-1 ---
__ufdi_rshift:
	cpl
	inc	a		; A = 1..23
__ufdi_rsh_lp:
	srl	e
	rr	b
	rr	c
	dec	a
	jr	nz, __ufdi_rsh_lp
	ldhl	sp, #2
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a
	ld	a, c
	ld	(hl+), a
	ld	a, b
	ld	(hl+), a
	ld	a, e
	ld	(hl), a

__ufdi_done:
	pop	hl		; return address
	add	sp, #2		; callee-cleanup: skip the sret slot
	jp	(hl)

__ufdi_saturate:
	ldhl	sp, #2
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a
	ld	d, #8
__ufdi_sat_lp:
	ld	a, #0xFF
	ld	(hl+), a
	dec	d
	jr	nz, __ufdi_sat_lp
	jr	__ufdi_done
