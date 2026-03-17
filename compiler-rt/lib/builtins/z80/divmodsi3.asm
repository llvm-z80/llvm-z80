; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl __udiv32_core
	.globl __udiv32_skip
	.globl __neg32_hlde
	.globl __neg32_divisor
	.globl ___udivsi3
	.globl ___divsi3
	.globl ___umodsi3
	.globl ___modsi3

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

;===------------------------------------------------------------------------===;
;=== 64-bit Integer Arithmetic ===============================================;
;===------------------------------------------------------------------------===;
;
; 64-bit division and modulo via restoring division algorithm.
; Memory-based: Z80 registers are too small for 64-bit operands.
;
; Calling convention (sret demotion, SDCC __sdcccall(1)):
;   Stack (caller pushes): sret pointer (2B), dividend (8B), divisor (8B)
;   sret pointer is at lowest address (pushed last by caller)
;
; Stack frame after push ix; ld ix,#0; add ix,sp; allocate 16:
;   IX-16..IX-9  = remainder (8 bytes, little-endian, IX-16 = LSB)
;   IX-8..IX-1   = quotient  (8 bytes, little-endian, IX-8  = LSB)
;   IX+0,1       = saved IX
;   IX+2,3       = return address
;   IX+4,5       = sret pointer (caller pushed on stack)
;   IX+6..IX+13  = dividend (8 bytes, little-endian, IX+6  = LSB)
;   IX+14..IX+21 = divisor  (8 bytes, little-endian, IX+14 = LSB)
;
;===------------------------------------------------------------------------===;

;===------------------------------------------------------------------------===;
