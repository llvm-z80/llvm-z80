; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___udivhi3

;===------------------------------------------------------------------------===;
; ___udivhi3 - 16-bit unsigned division (SM83)
;
; Input:  DE = dividend, BC = divisor
; Output: BC = quotient, HL = remainder
; Algorithm: restoring division with 8-bit divisor fast path
;===------------------------------------------------------------------------===;
___udivhi3:
	ld	a, b
	or	a
	jr	nz, ___udivhi3_16bit
	;; --- 8-bit divisor fast path (B == 0) ---
	;; HL serves as dividend shift register and quotient accumulator.
	;; E holds the 8-bit remainder. C is the divisor.
	;; B is the loop counter (starts at 16).
	ld	h, d
	ld	l, e		; HL = dividend
	ld	e, b		; E = 0 (initial remainder, B was 0)
	ld	b, #16		; loop counter
	add	hl, hl		; initial shift: dividend MSB -> carry
___udivhi3_8loop:
	ld	a, e		; get remainder
	rla			; remainder = remainder*2 + carry
	sub	c		; trial subtract
	jr	nc, ___udivhi3_8ok
	add	a, c		; restore
___udivhi3_8ok:
	ccf			; complement carry -> quotient bit
	ld	e, a		; save remainder
	; ADC HL,HL (SM83 has no native ADC HL,HL)
	ld	a, l
	adc	a, l
	ld	l, a
	ld	a, h
	adc	a, h
	ld	h, a
	dec	b
	jr	nz, ___udivhi3_8loop
	; HL = quotient, E = remainder, B = 0
	ld	b, h		; BC = quotient
	ld	c, l
	ld	l, e		; L = remainder
	ld	h, #0		; H = 0 (remainder fits in 8 bits)
	ret
	;; --- 16-bit divisor path ---
___udivhi3_16bit:
	; Swap DE<->BC: need BC=dividend, DE=divisor
	push	bc		; save divisor
	ld	b, d
	ld	c, e		; BC = dividend
	pop	de		; DE = divisor
	ld	hl, #0		; remainder
	ld	a, #16		; bit counter
___udivhi3_16loop:
	push	af		; save bit counter (A used for byte ops below)
	sla	c		; shift BC left (dividend MSB -> carry)
	rl	b
	; ADC HL,HL: remainder = remainder * 2 + carry (no native ADC HL,HL)
	ld	a, l
	adc	a, l
	ld	l, a
	ld	a, h
	adc	a, h
	ld	h, a
	jr	c, ___udivhi3_overflow	; 17-bit remainder >= divisor
	; SBC HL,DE: trial subtract (no native SBC HL,DE)
	ld	a, l
	sub	e
	ld	l, a
	ld	a, h
	sbc	a, d
	ld	h, a
	jr	nc, ___udivhi3_setbit
	; Restore remainder: ADD HL,DE
	add	hl, de
	jr	___udivhi3_next
___udivhi3_overflow:
	; 17-bit remainder, always >= divisor. SUB clears incoming carry.
	ld	a, l
	sub	e
	ld	l, a
	ld	a, h
	sbc	a, d
	ld	h, a
___udivhi3_setbit:
	inc	c		; set quotient bit 0
___udivhi3_next:
	pop	af		; restore bit counter
	dec	a
	jr	nz, ___udivhi3_16loop
	; BC = quotient, HL = remainder
	ret
