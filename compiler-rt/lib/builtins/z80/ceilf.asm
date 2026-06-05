; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _ceilf

;===------------------------------------------------------------------------===;
; _ceilf - Ceiling: round toward positive infinity
;
; Input:  HLDE = float
; Output: HLDE = ceil(x)
;
; Algorithm:
;   t = trunc(x)
;   if x > 0 and x != t: return t + 1.0
;   else: return t
;
; __addsf3 convention: arg1=HLDE, arg2=stack (callee-cleanup), returns HLDE.
;===------------------------------------------------------------------------===;
_ceilf:
	; Extract exponent
	ld	a, h
	add	a, a
	ld	b, a
	ld	a, l
	rlca
	and	#1
	or	b		; A = exponent

	; exp >= 150 → already integer
	cp	#150
	ret	nc

	; exp < 127 → |x| < 1.0
	cp	#127
	jr	c, __ceilf_small

	; 127 <= exp < 150: has fractional bits
	push	hl
	push	de

	; Truncate: mask off fractional bits
	sub	#127		; A = integer mantissa bits
	cp	#16
	jr	nc, __ceilf_hi
	cp	#8
	jr	nc, __ceilf_mid

	; A < 8: clear part of L, all of D, E
	ld	b, a
	ld	a, #7
	sub	b
	jr	z, __ceilf_l_done
	ld	b, a
	ld	a, #0xFF
__ceilf_l_lp:
	sla	a
	djnz	__ceilf_l_lp
	and	l
	ld	l, a
__ceilf_l_done:
	ld	d, #0
	ld	e, #0
	jr	__ceilf_check

__ceilf_mid:
	sub	#8
	jr	z, __ceilf_mid_zero
	ld	b, a
	ld	a, #8
	sub	b
	ld	b, a
	ld	a, #0xFF
__ceilf_m_lp:
	sla	a
	djnz	__ceilf_m_lp
	and	d
	ld	d, a
	ld	e, #0
	jr	__ceilf_check
__ceilf_mid_zero:
	ld	e, #0
	jr	__ceilf_check

__ceilf_hi:
	sub	#16
	jr	z, __ceilf_check
	ld	b, a
	ld	a, #8
	sub	b
	ld	b, a
	ld	a, #0xFF
__ceilf_h_lp:
	sla	a
	djnz	__ceilf_h_lp
	and	e
	ld	e, a

__ceilf_check:
	; HLDE = trunc(x). Stack has original.
	; If negative or trunc==original, return trunc
	bit	7, h
	jr	nz, __ceilf_done	; negative → trunc == ceil

	; Positive: check if trunc(x) != x
	pop	bc		; BC = original DE
	ld	a, e
	xor	c
	jr	nz, __ceilf_add1
	ld	a, d
	xor	b
	jr	nz, __ceilf_add1
	pop	bc		; BC = original HL
	ld	a, l
	xor	c
	jr	nz, __ceilf_add1b
	ld	a, h
	xor	b
	jr	nz, __ceilf_add1b
	; trunc == original
	ret

__ceilf_add1:
	pop	bc		; discard original HL
__ceilf_add1b:
	; trunc(x) != x and x > 0: return trunc(x) + 1.0
	; __addsf3(trunc(x), 1.0): arg1=HLDE, arg2=1.0 on stack
	ld	bc, #0x3F80	; B=H2=0x3F, C=L2=0x80
	push	bc
	ld	bc, #0x0000	; B=D2=0x00, C=E2=0x00
	push	bc
	call	___addsf3	; callee-cleanup removes 4 bytes
	ret

__ceilf_done:
	pop	bc
	pop	bc
	ret

__ceilf_small:
	; |x| < 1.0
	bit	7, h
	jr	nz, __ceilf_neg0
	; Positive and nonzero: ceil = 1.0
	ld	a, h
	and	#0x7F
	or	l
	or	d
	or	e
	jr	z, __ceilf_pzero
	ld	hl, #0x3F80	; 1.0f
	ld	de, #0x0000
	ret
__ceilf_pzero:
	; +0.0 → return +0.0
	ret
__ceilf_neg0:
	; Negative: ceil = -0.0
	ld	h, #0x80
	ld	l, #0
	ld	d, l
	ld	e, l
	ret
