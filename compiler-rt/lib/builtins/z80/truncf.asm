; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _truncf

;===------------------------------------------------------------------------===;
; _truncf - Truncate float toward zero (remove fractional part)
;
; Input:  HLDE = float
; Output: HLDE = truncated float
;
; IEEE 754 single precision: sign(1) | exponent(8) | mantissa(23)
;   H[7]=sign, H[6:0]=exp[7:1], L[7]=exp[0], L[6:0]:D:E=mantissa[22:0]
;
; Algorithm:
;   - If exp < 127 (|x| < 1.0): return ±0.0
;   - If exp >= 150 (|x| >= 2^23): already integer, return x
;   - Otherwise: mask off fractional bits from mantissa
;===------------------------------------------------------------------------===;
_truncf:
	; Extract exponent
	ld	a, h
	add	a, a		; A = exp[7:1] << 1, carry = sign
	ld	b, a
	ld	a, l
	rlca
	and	#1
	or	b		; A = full exponent (8 bits)

	; exp < 127 → |x| < 1.0 → return ±0.0
	cp	#127
	jr	c, __truncf_zero

	; exp >= 150 → all 23 mantissa bits are integer part → return x
	cp	#150
	ret	nc

	; 127 <= exp < 150: mask off (150 - exp) fractional bits
	; Fractional bits to clear: 150 - exp
	; Mantissa is in L[6:0]:D:E (23 bits)
	; Bits 22..16 in L[6:0], bits 15..8 in D, bits 7..0 in E
	sub	#127		; A = exp - 127 = number of integer mantissa bits (0..22)
	; We need to keep A integer bits and zero the rest (23-A bits)

	; If A >= 16: only E and part of D are fractional
	cp	#16
	jr	nc, __truncf_hi

	; If A >= 8: E is all fractional, part of D is fractional
	cp	#8
	jr	nc, __truncf_mid

	; A < 8: E and D are all fractional, part of L is fractional
	; Keep A bits in L[6:0], clear rest of L and all of D,E
	; Integer bits are in L[6 .. 6-(A-1)] (top A bits of L's mantissa)
	; Mask: 0xFF << (7 - A) for L, but we need mantissa-only (L[6:0])
	ld	b, a		; B = integer bit count
	ld	a, #0xFF
	; Shift mask: clear bottom (7-B) bits of the 7 mantissa bits in L
	; We need to clear bits (6-B)..0, i.e., mask = ~((1 << (7-B)) - 1)
	; Equivalently: mask with 0xFF << (7-B) then AND with 0x80|result to keep L[7]
	push	hl
	ld	c, #7
	ld	a, c
	sub	b		; A = 7 - B = bits to clear in L
	ld	b, a
	ld	a, #0xFF
	jr	z, __truncf_lmask_done
__truncf_lmask_lp:
	sla	a		; shift mask left, clearing low bits
	djnz	__truncf_lmask_lp
__truncf_lmask_done:
	pop	hl
	and	l
	ld	l, a
	ld	d, #0
	ld	e, #0
	ret

__truncf_mid:
	; 8 <= A < 16: E is all fractional, clear part of D
	; Integer mantissa bits in D: A - 8, bits to clear: 16 - A = 8-(A-8)
	sub	#8		; A = integer bits in D (0..7)
	ld	b, a
	ld	a, #0xFF
	ld	c, a		; will use if B=0
	or	a
	jr	z, __truncf_dmask_done
	ld	c, #8
	ld	a, c
	sub	b		; A = bits to clear in D
	ld	b, a
	ld	a, #0xFF
__truncf_dmask_lp:
	sla	a
	djnz	__truncf_dmask_lp
__truncf_dmask_done:
	and	d
	ld	d, a
	ld	e, #0
	ret

__truncf_hi:
	; 16 <= A < 23: D and L are all integer, clear part of E
	sub	#16		; A = integer bits in E (0..6)
	ld	b, a
	ld	a, #8
	sub	b		; A = bits to clear in E
	ld	b, a
	ld	a, #0xFF
	jr	z, __truncf_ret	; nothing to clear
__truncf_emask_lp:
	sla	a
	djnz	__truncf_emask_lp
	and	e
	ld	e, a
__truncf_ret:
	ret

__truncf_zero:
	; Return ±0.0 (preserve sign)
	ld	a, h
	and	#0x80		; keep sign
	ld	h, a
	ld	l, #0
	ld	d, l
	ld	e, l
	ret
