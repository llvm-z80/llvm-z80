; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _floorf

;===------------------------------------------------------------------------===;
; _floorf - Floor: round toward negative infinity
;
; Input:  HLDE = float
; Output: HLDE = floor(x)
;
; Algorithm:
;   t = trunc(x)
;   if x < 0 and x != t: return t - 1.0
;   else: return t
;
; __subsf3 convention: arg1=HLDE, arg2=stack (callee-cleanup), returns HLDE.
;===------------------------------------------------------------------------===;
_floorf:
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
	jr	c, __floorf_small

	; 127 <= exp < 150: has fractional bits
	; Save original to check if truncation changed it
	push	hl
	push	de

	; Compute trunc(x) inline - mask off fractional bits
	sub	#127		; A = integer mantissa bits (0..22)
	cp	#16
	jr	nc, __floorf_hi
	cp	#8
	jr	nc, __floorf_mid

	; A < 8: clear part of L, all of D, E
	ld	b, a
	ld	a, #7
	sub	b
	jr	z, __floorf_l_done
	ld	b, a
	ld	a, #0xFF
__floorf_l_lp:
	sla	a
	djnz	__floorf_l_lp
	and	l
	ld	l, a
__floorf_l_done:
	ld	d, #0
	ld	e, #0
	jr	__floorf_check

__floorf_mid:
	sub	#8
	jr	z, __floorf_mid_zero
	ld	b, a
	ld	a, #8
	sub	b
	ld	b, a
	ld	a, #0xFF
__floorf_m_lp:
	sla	a
	djnz	__floorf_m_lp
	and	d
	ld	d, a
	ld	e, #0
	jr	__floorf_check
__floorf_mid_zero:
	ld	e, #0
	jr	__floorf_check

__floorf_hi:
	sub	#16
	jr	z, __floorf_check
	ld	b, a
	ld	a, #8
	sub	b
	ld	b, a
	ld	a, #0xFF
__floorf_h_lp:
	sla	a
	djnz	__floorf_h_lp
	and	e
	ld	e, a

__floorf_check:
	; HLDE = trunc(x). Stack has original (HL, DE).
	; If positive or trunc==original, return trunc
	bit	7, h
	jr	z, __floorf_done	; positive → trunc == floor

	; Negative: check if trunc(x) != x (had fractional part)
	pop	bc		; BC = original DE
	ld	a, e
	xor	c
	jr	nz, __floorf_sub1
	ld	a, d
	xor	b
	jr	nz, __floorf_sub1
	pop	bc		; BC = original HL
	ld	a, l
	xor	c
	jr	nz, __floorf_sub1b
	ld	a, h
	xor	b
	jr	nz, __floorf_sub1b
	; trunc == original, no fractional part
	ret

__floorf_sub1:
	pop	bc		; discard original HL
__floorf_sub1b:
	; trunc(x) != x and x < 0: return trunc(x) - 1.0
	; __subsf3(trunc(x), 1.0): arg1=HLDE=trunc(x), arg2=1.0 on stack
	; 1.0f = 0x3F800000 → H=0x3F, L=0x80, D=0x00, E=0x00
	ld	bc, #0x3F80	; H2:L2 of 1.0f
	push	bc
	ld	bc, #0x0000	; D2:E2 of 1.0f
	push	bc
	call	___subsf3	; callee-cleanup removes 4 bytes
	ret

__floorf_done:
	pop	bc		; discard original DE
	pop	bc		; discard original HL
	ret

__floorf_small:
	; |x| < 1.0
	bit	7, h
	jr	nz, __floorf_neg1
	; Positive: floor = +0.0
	ld	h, #0
	ld	l, h
	ld	d, h
	ld	e, h
	ret
__floorf_neg1:
	; Negative: floor = -1.0 (unless -0.0)
	ld	a, h
	and	#0x7F
	or	l
	or	d
	or	e
	jr	z, __floorf_nzero
	ld	hl, #0xBF80
	ld	de, #0x0000
	ret
__floorf_nzero:
	ld	h, #0x80
	ld	l, #0
	ld	d, l
	ld	e, l
	ret
