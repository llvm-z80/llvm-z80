; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___udivhi3
	.globl ___udivmodhi4

;===------------------------------------------------------------------------===;
; ___udivhi3 - 16-bit unsigned division
; ___udivmodhi4 - the same routine, named for both of its results
;
; Input:  HL = dividend, DE = divisor
; Output: DE = quotient, HL = remainder
; Algorithm: restoring division with 8-bit divisor fast path
;
; ___udivhi3 promises only the quotient. A quotient and a remainder of the
; same operands are fused into one call to ___udivmodhi4.
;===------------------------------------------------------------------------===;
___udivhi3:
___udivmodhi4:
	ld	a, e
	and	#0x80
	or	d
	jr	nz, ___udivhi3_16bit
	;; --- 8-bit divisor fast path (divisor < 128) ---
	;; HL serves as both dividend shift register and quotient accumulator.
	;; A is the remainder. It is doubled before each trial subtraction, so it
	;; has to stay below 128: a divisor of 128 or more goes the 16-bit way.
	;; Quotient bits enter HL via carry + ADC HL,HL.
	; a is already 0
	ld	b, #16		; iteration counter
	add	hl, hl		; initial shift: dividend MSB -> carry
___udivhi3_8loop:
	rla			; remainder = remainder*2 + carry
	sub	e		; trial subtract
	jr	nc, ___udivhi3_8ok
	add	a, e		; restore
___udivhi3_8ok:
	ccf			; complement carry -> quotient bit
	adc	hl, hl		; shift HL left + quotient bit into L[0]
	djnz	___udivhi3_8loop
	; HL = quotient, A = remainder, D = 0
	ld	e, a		; E = remainder
	ex	de, hl		; DE = quotient, HL = 0:remainder
	ret
	;; --- 16-bit divisor path ---
___udivhi3_16bit:
	ld	b, h
	ld	c, l		; BC = dividend (becomes quotient)
	ld	hl, #0		; remainder
	ld	a, #16		; bit counter
___udivhi3_16loop:
	sla	c		; shift BC left
	rl	b		; MSB -> carry
	adc	hl, hl		; remainder = remainder * 2 + carry
	jr	c, ___udivhi3_overflow	; 17-bit remainder, always >= divisor
	sbc	hl, de		; trial subtract (carry = 0 here)
	jr	nc, ___udivhi3_setbit
	add	hl, de		; restore remainder
	jr	___udivhi3_next
___udivhi3_overflow:
	or	a		; clear carry
	sbc	hl, de		; subtract (always fits)
___udivhi3_setbit:
	inc	c		; set quotient bit 0
___udivhi3_next:
	dec	a
	jr	nz, ___udivhi3_16loop
	ld	d, b
	ld	e, c		; DE = quotient
	ret
