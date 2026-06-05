; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl __mul8x8
	.globl __mul8x8_loop
	.globl __mul8x8_skip
	.globl ___umulhi3

;===------------------------------------------------------------------------===;

;===------------------------------------------------------------------------===;
; __mul8x8 - 8-bit multiply helper
;
; Input:  H = multiplicand, E = multiplier
; Output: HL = H * E (16-bit result)
; Clobbers: A, B, D
; Preserves: C, E
;===------------------------------------------------------------------------===;
__mul8x8:
	ld	d, h		; D = multiplicand
	ld	h, #0
	ld	l, h		; HL = 0 (accumulator)
	ld	b, #8
__mul8x8_loop:
	add	hl, hl		; product <<= 1
	sla	d		; multiplicand MSB → carry
	jr	nc, __mul8x8_skip
	ld	a, l
	add	a, e		; low byte += multiplier
	ld	l, a
	jr	nc, __mul8x8_skip
	inc	h
__mul8x8_skip:
	dec	b
	jr	nz, __mul8x8_loop
	ret

;===------------------------------------------------------------------------===;
; ___umulhi3 - Upper 16 bits of 16-bit multiply
;
; Input:  DE = [a_hi:a_lo], BC = [b_hi:b_lo]
; Output: BC = upper 16 bits of (DE * BC)
; Algorithm: 4 partial 8×8 products via __mul8x8.
;   p0=a_lo*b_lo, p1=a_hi*b_lo, p2=a_lo*b_hi, p3=a_hi*b_hi
;   byte1 = p0_hi + p1_lo + p2_lo  (only carry propagates)
;   byte2 = p3_lo + p1_hi + p2_hi + carry_from_byte1
;   byte3 = p3_hi + carry_from_byte2
;   Result = byte3:byte2
;===------------------------------------------------------------------------===;
___umulhi3:
	push	de		; save [a_hi:a_lo]
	push	bc		; save [b_hi:b_lo]
	; Stack: SP+0=b_lo, SP+1=b_hi, SP+2=a_lo, SP+3=a_hi

	; p0 = a_lo * b_lo
	ldhl	sp, #0
	ld	e, (hl)		; E = b_lo
	ldhl	sp, #2
	ld	h, (hl)		; H = a_lo
	call	__mul8x8
	push	hl		; save p0
	; Stack: SP+0=p0(2), SP+2=b(2), SP+4=a(2)

	; p1 = a_hi * b_lo
	ldhl	sp, #2
	ld	e, (hl)		; E = b_lo
	ldhl	sp, #5
	ld	h, (hl)		; H = a_hi
	call	__mul8x8
	push	hl		; save p1
	; Stack: SP+0=p1(2), SP+2=p0(2), SP+4=b(2), SP+6=a(2)

	; p2 = a_lo * b_hi
	ldhl	sp, #5
	ld	e, (hl)		; E = b_hi
	ldhl	sp, #6
	ld	h, (hl)		; H = a_lo
	call	__mul8x8
	push	hl		; save p2
	; Stack: SP+0=p2(2), SP+2=p1(2), SP+4=p0(2), SP+6=b(2), SP+8=a(2)

	; p3 = a_hi * b_hi
	ldhl	sp, #7
	ld	e, (hl)		; E = b_hi
	ldhl	sp, #9
	ld	h, (hl)		; H = a_hi
	call	__mul8x8
	; HL = p3, all products done

	; === Accumulate result ===
	ld	b, h		; B = p3_hi (byte3)
	ld	c, l		; C = p3_lo (byte2)

	; byte2 += p1_hi
	ldhl	sp, #3
	ld	a, (hl)		; A = p1_hi
	add	a, c
	ld	c, a
	ld	a, b
	adc	a, #0
	ld	b, a

	; byte2 += p2_hi
	ldhl	sp, #1
	ld	a, (hl)		; A = p2_hi
	add	a, c
	ld	c, a
	ld	a, b
	adc	a, #0
	ld	b, a

	; byte1 carry: p0_hi + p1_lo + p2_lo
	ldhl	sp, #5
	ld	a, (hl)		; A = p0_hi
	ldhl	sp, #2
	add	a, (hl)		; A += p1_lo, CF1
	ld	d, a		; D = partial byte1
	ld	a, #0
	adc	a, #0		; A = CF1
	ld	e, a		; E = carry1
	ld	a, d
	ldhl	sp, #0
	add	a, (hl)		; A += p2_lo, CF2
	ld	a, e		; A = carry1 (ld preserves flags)
	adc	a, #0		; A = carry1 + CF2 = total carry (0-2)

	; byte2 += total carry
	add	a, c
	ld	c, a
	ld	a, b
	adc	a, #0
	ld	b, a

	add	sp, #10		; clean: p2+p1+p0+b+a = 5 pushes
	ret

;===------------------------------------------------------------------------===;
; __neg32_debc - Negate 32-bit value in DEBC
;
; DEBC = -DEBC (two's complement)
