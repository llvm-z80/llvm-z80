; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___fixunssfti

;===------------------------------------------------------------------------===;
; ___fixunssfti - Convert float to unsigned int128 (SM83)
;
; Calling convention (sret demotion, SDCC __sdcccall(1)):
;   Input:  DEBC = float (D=sign+exp_hi, E=exp_lo+mant_hi, B=mant_mid, C=mant_lo)
;   Stack:  SP+0,1 = return address, SP+2,3 = sret pointer
;   Output: 16 bytes at *sret, little-endian; callee pops the sret slot
;
; The signed sibling without the negation: below zero and NaN answer zero, and
; only an infinity saturates. The largest exponent is given its own tail, since
; there the mantissa lands on the last three bytes and the fourth store would
; run past the end.
;===------------------------------------------------------------------------===;
___fixunssfti:
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

	bit	7, d
	jp	nz, __ufti_done	; negative -> 0

	; Exponent into D
	ld	a, d
	add	a, a
	ld	d, a
	ld	a, e
	rlca
	and	#1
	or	d
	ld	d, a		; D = exponent

	inc	a		; A = 0 iff exp = 255
	jr	nz, __ufti_finite
	ld	a, e
	and	#0x7F
	or	b
	or	c
	jr	nz, __ufti_done	; NaN -> 0
	jr	__ufti_saturate

__ufti_finite:
	ld	a, d
	cp	#127
	jr	c, __ufti_done	; |value| < 1.0 -> 0

	set	7, e		; 24-bit mantissa in E:B:C

	ld	a, d
	cp	#254
	jr	z, __ufti_top

	sub	#150
	jr	c, __ufti_rshift

	; --- Left shift path: shift in 0..103 ---
	push	af		; save shift
	ld	d, #0		; D = fourth (top) byte
	and	#7
	jr	z, __ufti_lsh_done
	ld	l, a
__ufti_lsh_lp:
	sla	c
	rl	b
	rl	e
	rl	d
	dec	l
	jr	nz, __ufti_lsh_lp
__ufti_lsh_done:
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
	jr	__ufti_done

__ufti_top:
	; exp 254: the mantissa moves a whole thirteen bytes up and fills the
	; last three, leaving nothing for a fourth byte to carry.
	ldhl	sp, #2
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a
	ld	a, l
	add	a, #13
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
	jr	__ufti_done

	; --- Right shift path: shift in -23..-1 ---
__ufti_rshift:
	cpl
	inc	a		; A = 1..23
__ufti_rsh_lp:
	srl	e
	rr	b
	rr	c
	dec	a
	jr	nz, __ufti_rsh_lp
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

__ufti_done:
	pop	hl		; return address
	add	sp, #2		; callee-cleanup: skip the sret slot
	jp	(hl)

__ufti_saturate:
	ldhl	sp, #2
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a
	ld	d, #16
__ufti_sat_lp:
	ld	a, #0xFF
	ld	(hl+), a
	dec	d
	jr	nz, __ufti_sat_lp
	jr	__ufti_done
