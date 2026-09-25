; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl __udiv32_core
	.globl ___udivsi3
	.globl ___divsi3
	.globl ___umodsi3
	.globl ___modsi3
	.globl ___udivmodsi4
	.globl __neg32_hlde
	.globl __neg32_divisor

;===------------------------------------------------------------------------===;
; __udiv32_core - Core 32-bit unsigned division subroutine
;
; Preconditions:
;   HLDE = dividend (becomes quotient)
;   B = 32 (iteration count)
;   Shadow HL':DE' = 0 (remainder)
;   IX+4..IX+7 = divisor (caller's frame)
;
; Postconditions:
;   HLDE = quotient
;   Shadow HL':DE' = remainder
;===------------------------------------------------------------------------===;
__udiv32_core:
	; Shift quotient left, MSB into carry
	sla	e
	rl	d
	rl	l
	rl	h

	; Shift carry into remainder (shadow regs)
	exx
	rl	e
	rl	d
	rl	l
	rl	h

	; Compare remainder with divisor (non-destructive)
	ld	a, e
	sub	4(ix)
	ld	a, d
	sbc	a, 5(ix)
	ld	a, l
	sbc	a, 6(ix)
	ld	a, h
	sbc	a, 7(ix)
	jr	c, __udiv32_skip	; remainder < divisor

	; remainder >= divisor: subtract
	ld	a, e
	sub	4(ix)
	ld	e, a
	ld	a, d
	sbc	a, 5(ix)
	ld	d, a
	ld	a, l
	sbc	a, 6(ix)
	ld	l, a
	ld	a, h
	sbc	a, 7(ix)
	ld	h, a

	; Set quotient bit (switch to main, set, switch back)
	exx
	set	0, e
	exx

__udiv32_skip:
	exx			; back to main registers
	djnz	__udiv32_core
	ret

;===------------------------------------------------------------------------===;
; 32-bit negate HLDE helper (used by signed div/mod)
; Negates HLDE in place: HLDE = -HLDE
;===------------------------------------------------------------------------===;
__neg32_hlde:
	ld	a, e
	cpl
	ld	e, a
	ld	a, d
	cpl
	ld	d, a
	ld	a, l
	cpl
	ld	l, a
	ld	a, h
	cpl
	ld	h, a
	inc	e
	ret	nz
	inc	d
	ret	nz
	inc	l
	ret	nz
	inc	h
	ret

;===------------------------------------------------------------------------===;
; Negate divisor at IX+4..IX+7 in place
;===------------------------------------------------------------------------===;
__neg32_divisor:
	ld	a, 4(ix)
	cpl
	ld	4(ix), a
	ld	a, 5(ix)
	cpl
	ld	5(ix), a
	ld	a, 6(ix)
	cpl
	ld	6(ix), a
	ld	a, 7(ix)
	cpl
	ld	7(ix), a
	inc	4(ix)
	ret	nz
	inc	5(ix)
	ret	nz
	inc	6(ix)
	ret	nz
	inc	7(ix)
	ret

;===------------------------------------------------------------------------===;
; ___udivsi3 - Unsigned 32-bit division
;
; Input:  HLDE = dividend, stack IX+4..IX+7 = divisor
; Output: HLDE = quotient
;===------------------------------------------------------------------------===;
___udivsi3:
	push	ix
	ld	ix, #0
	add	ix, sp
	exx
	ld	hl, #0
	ld	de, #0
	exx
	ld	b, #32
	call	__udiv32_core
	pop	ix
	ret

;===------------------------------------------------------------------------===;
; ___divsi3 - Signed 32-bit division
;
; Input:  HLDE = dividend, stack IX+4..IX+7 = divisor
; Output: HLDE = quotient
;===------------------------------------------------------------------------===;
___divsi3:
	push	ix
	ld	ix, #0
	add	ix, sp

	; Result sign = XOR of input signs
	ld	a, h
	xor	7(ix)
	push	af		; save result sign (bit 7)

	; Make dividend positive
	bit	7, h
	call	nz, __neg32_hlde

	; Make divisor positive
	bit	7, 7(ix)
	call	nz, __neg32_divisor

	; Unsigned divide
	exx
	ld	hl, #0
	ld	de, #0
	exx
	ld	b, #32
	call	__udiv32_core

	; Apply result sign
	pop	af
	bit	7, a
	call	nz, __neg32_hlde

	pop	ix
	ret

;===------------------------------------------------------------------------===;
; ___umodsi3 - Unsigned 32-bit modulo
;
; Input:  HLDE = dividend, stack IX+4..IX+7 = divisor
; Output: HLDE = remainder
;===------------------------------------------------------------------------===;
___umodsi3:
	push	ix
	ld	ix, #0
	add	ix, sp
	exx
	ld	hl, #0
	ld	de, #0
	exx
	ld	b, #32
	call	__udiv32_core
	; Remainder is in shadow HL':DE' → move to HLDE
	exx
	push	hl
	push	de
	exx
	pop	de
	pop	hl
	pop	ix
	ret

;===------------------------------------------------------------------------===;
; ___udivmodsi4 - Unsigned 32-bit fused divide + modulo (compiler-rt ABI)
;
; One __udiv32_core pass yields BOTH results, so the quotient and remainder
; cost a single division instead of the two separate __udivsi3 + __umodsi3
; calls the compiler used to emit for an adjacent x/y, x%y pair.
;
; Input:  HLDE = dividend
;         stack IX+4..7 = divisor
;         stack IX+8..9 = pointer to a 4-byte remainder buffer (caller-owned)
; Output: HLDE = quotient (return value)
;         *(IX+8..9) = remainder (4 bytes, little-endian)
;===------------------------------------------------------------------------===;
___udivmodsi4:
	push	ix
	ld	ix, #0
	add	ix, sp
	exx
	ld	hl, #0
	ld	de, #0
	exx
	ld	b, #32
	call	__udiv32_core		; HLDE = quotient, shadow HL':DE' = remainder

	; Stash the quotient so HLDE is free to assemble the remainder.
	push	hl
	push	de

	; Move the remainder out of the shadow registers into main HLDE.
	exx
	push	hl			; remainder high word (shadow HL')
	push	de			; remainder low  word (shadow DE')
	exx
	pop	de			; DE = remainder low
	pop	hl			; HL = remainder high  -> HLDE = remainder

	; Store the 4-byte remainder through the caller's pointer (IX+8..9).
	ld	c, 8(ix)
	ld	b, 9(ix)		; BC = &remainder
	ld	a, e
	ld	(bc), a
	inc	bc
	ld	a, d
	ld	(bc), a
	inc	bc
	ld	a, l
	ld	(bc), a
	inc	bc
	ld	a, h
	ld	(bc), a

	; Restore the quotient as the return value.
	pop	de
	pop	hl			; HLDE = quotient
	pop	ix
	ret

;===------------------------------------------------------------------------===;
; ___modsi3 - Signed 32-bit modulo
;
; Input:  HLDE = dividend, stack IX+4..IX+7 = divisor
; Output: HLDE = remainder (sign follows dividend)
;===------------------------------------------------------------------------===;
___modsi3:
	push	ix
	ld	ix, #0
	add	ix, sp

	; Remainder sign = dividend sign
	ld	a, h
	push	af

	; Make dividend positive
	bit	7, h
	call	nz, __neg32_hlde

	; Make divisor positive
	bit	7, 7(ix)
	call	nz, __neg32_divisor

	; Unsigned divide
	exx
	ld	hl, #0
	ld	de, #0
	exx
	ld	b, #32
	call	__udiv32_core

	; Get remainder from shadow regs
	exx
	push	hl
	push	de
	exx
	pop	de
	pop	hl

	; Apply dividend sign to remainder
	pop	af
	bit	7, a
	call	nz, __neg32_hlde

	pop	ix
	ret
