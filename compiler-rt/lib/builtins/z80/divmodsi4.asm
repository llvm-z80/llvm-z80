; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___divmodsi4

; Externals defined in divmodsi3.asm (the unsigned core + 32-bit negate
; helpers).  Kept in a separate object so unsigned-only code (which links
; ___udivmodsi4 / the core from divmodsi3) does not drag in this signed
; routine -- whole-object linking would otherwise bundle it.

;===------------------------------------------------------------------------===;
; ___divmodsi4 - Signed 32-bit fused divide + modulo (compiler-rt ABI)
;
; Signed counterpart of ___udivmodsi4: one __udiv32_core pass on the
; magnitudes, then the C sign rules are applied -- quotient sign is the XOR
; of the operand signs, remainder sign follows the dividend.
;
; Input:  HLDE = dividend
;         stack IX+4..7 = divisor
;         stack IX+8..9 = pointer to a 4-byte remainder buffer (caller-owned)
; Output: HLDE = quotient (return value)
;         *(IX+8..9) = remainder (4 bytes, little-endian)
;===------------------------------------------------------------------------===;
___divmodsi4:
	push	ix
	ld	ix, #0
	add	ix, sp

	; Result-sign bookkeeping, from the ORIGINAL operand signs.
	ld	a, h
	xor	7(ix)
	push	af			; quotient sign: bit 7 = sign(dividend) ^ sign(divisor)
	ld	a, h
	push	af			; remainder sign: bit 7 = sign(dividend)

	; Make dividend and divisor non-negative.
	bit	7, h
	call	nz, __neg32_hlde
	bit	7, 7(ix)
	call	nz, __neg32_divisor

	; Unsigned core: HLDE = |quotient|, shadow HL':DE' = |remainder|.
	exx
	ld	hl, #0
	ld	de, #0
	exx
	ld	b, #32
	call	__udiv32_core

	; Remainder first: park |quotient| in the shadow set, bring |rem| to main.
	exx				; main = |remainder|, shadow = |quotient|
	pop	af			; A bit 7 = remainder sign
	bit	7, a
	call	nz, __neg32_hlde	; main HLDE = signed remainder

	; Store the 4-byte remainder through the caller pointer (IX+8..9).
	ld	c, 8(ix)
	ld	b, 9(ix)
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

	; Quotient: bring |quotient| back, apply its sign, return it.
	exx				; main = |quotient|
	pop	af			; A bit 7 = quotient sign
	bit	7, a
	call	nz, __neg32_hlde	; main HLDE = signed quotient
	pop	ix
	ret
