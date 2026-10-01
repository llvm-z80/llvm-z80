; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___udivsi3
	.globl ___umodsi3
	.globl ___divsi3
	.globl ___modsi3
	.globl __udivmodsi
	.globl __divmodsi_abs
	.globl __divmodsi_neg

;===------------------------------------------------------------------------===;
; ___udivsi3 - 32-bit unsigned division
; ___umodsi3 - 32-bit unsigned remainder
; ___divsi3  - 32-bit signed division
; ___modsi3  - 32-bit signed remainder
;
; Input:  HLDE = dividend, stack = divisor
; Output: HLDE = result; the caller pops the divisor
;
; The signed forms negate the divisor in its stack slot. __udivmodsi and the
; sign helpers are shared with udivmodsi4.asm and divmodsi4.asm.
;===------------------------------------------------------------------------===;
___udivsi3:
	ld	c, #0
	jr	__divmodsi
___umodsi3:
	ld	c, #1
	jr	__divmodsi
___divsi3:
	ld	c, #2
	jr	__divmodsi
___modsi3:
	ld	c, #3
; C = mode: bit 0 = return the remainder, bit 1 = signed.
__divmodsi:
	push	ix
	ld	ix, #0
	add	ix, sp
	bit	1, c
	jr	z, __divmodsi_go
	ld	a, h
	bit	0, c
	jr	nz, __divmodsi_sign
	xor	7(ix)
__divmodsi_sign:
	and	#0x80
	or	c
	ld	c, a		; bit 7: negate the result
	call	__divmodsi_abs
__divmodsi_go:
	push	bc
	call	__udivmodsi
	pop	af		; F = mode: carry = bit 0, S = bit 7
	jr	nc, __divmodsi_q
	push	iy
	pop	hl
	ld	d, b
	ld	e, c
__divmodsi_q:
	call	m, __divmodsi_neg
	pop	ix
	ret

; Replace the dividend and the divisor with their magnitudes. Preserves C.
__divmodsi_abs:
	bit	7, 7(ix)
	jr	z, __divmodsi_absn
	xor	a
	ld	b, a
	sub	4(ix)
	ld	4(ix), a
	ld	a, b
	sbc	a, 5(ix)
	ld	5(ix), a
	ld	a, b
	sbc	a, 6(ix)
	ld	6(ix), a
	ld	a, b
	sbc	a, 7(ix)
	ld	7(ix), a
__divmodsi_absn:
	bit	7, h
	ret	z
	; fall through

; Negate HLDE. Preserves C.
__divmodsi_neg:
	xor	a
	ld	b, a
	sub	e
	ld	e, a
	ld	a, b
	sbc	a, d
	ld	d, a
	ld	a, b
	sbc	a, l
	ld	l, a
	ld	a, b
	sbc	a, h
	ld	h, a
	ret

;===------------------------------------------------------------------------===;
; __udivmodsi - Unsigned 32-bit division core
;
; Input:  HLDE = dividend, IX+4..IX+7 = divisor
; Output: HLDE = quotient, IY:BC = remainder
;
; divisor < 2^16: ___udivmodhi4 on the high half, then 16 steps over the low
;   half with a 16-bit remainder. A:C collects the quotient bits inverted.
; divisor >= 2^16: the quotient fits in 16 bits, so 16 steps starting from
;   the high half as the remainder.
;===------------------------------------------------------------------------===;
__udivmodsi:
	ld	a, 6(ix)
	or	7(ix)
	jr	nz, __udivmodsi_big
	ld	iy, #0		; quotient high half
	push	de
	ld	e, 4(ix)
	ld	d, 5(ix)
	ld	a, l
	sub	e
	ld	a, h
	sbc	a, d
	jr	c, __udivmodsi_lo	; high half < divisor
	call	___udivmodhi4	; DE = quotient high, HL = remainder
	push	de
	pop	iy
	ld	e, 4(ix)
	ld	d, 5(ix)
__udivmodsi_lo:
	pop	bc
	ld	a, b		; A:C = dividend low
	ld	b, #16
__udivmodsi_loop:
	rl	c
	rla
	adc	hl, hl
	jr	c, __udivmodsi_over
	sbc	hl, de
	jr	nc, __udivmodsi_next
	add	hl, de		; restore; sets carry
__udivmodsi_next:
	djnz	__udivmodsi_loop
	rl	c
	rla
	cpl
	ld	d, a
	ld	a, c
	cpl
	ld	e, a
	ld	b, h
	ld	c, l
	push	iy
	pop	hl
	ld	iy, #0
	ret
__udivmodsi_over:
	or	a		; a 17-bit remainder always exceeds the divisor
	sbc	hl, de
	or	a
	jr	__udivmodsi_next

__udivmodsi_big:
	push	de
	pop	iy
	ld	de, #0		; DE:HL = remainder
	ld	c, 4(ix)
	ld	b, #16
__udivmodsi_bigloop:
	add	iy, iy
	adc	hl, hl
	rl	e
	rl	d
	jr	c, __udivmodsi_sub	; 33-bit remainder
	ld	a, l
	sub	c
	ld	a, h
	sbc	a, 5(ix)
	ld	a, e
	sbc	a, 6(ix)
	ld	a, d
	sbc	a, 7(ix)
	jr	c, __udivmodsi_bignext
__udivmodsi_sub:
	ld	a, l
	sub	c
	ld	l, a
	ld	a, h
	sbc	a, 5(ix)
	ld	h, a
	ld	a, e
	sbc	a, 6(ix)
	ld	e, a
	ld	a, d
	sbc	a, 7(ix)
	ld	d, a
	inc	iy
__udivmodsi_bignext:
	djnz	__udivmodsi_bigloop
	ld	b, h
	ld	c, l
	push	iy
	push	de
	pop	iy
	pop	de
	ld	hl, #0
	ret
