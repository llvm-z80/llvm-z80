; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _fmodf

;===------------------------------------------------------------------------===;
; _fmodf - Float remainder: a - trunc(a/b) * b  (SM83)
;
; Input:  DEBC = a, stack = b  (callee-cleanup)
; Output: DEBC = fmod(a, b)
;
; Approach: save a and b in a local stack frame, call div/trunc/mul/sub.
; All binary float functions callee-cleanup their 4-byte arg2.
;
; Frame layout (SP-relative after all saves):
;   SP+0..3: saved_b (C,B,E,D)
;   SP+4..7: saved_a (C,B,E,D)
;   SP+8..9: return address
;   SP+10..13: original b (arg2, callee-cleanup by us)
;===------------------------------------------------------------------------===;
_fmodf:
	; Save a
	push	de		; a high (E at SP, D at SP+1)
	push	bc		; a low  (C at SP, B at SP+1)
	; SP+0..1: a_C,a_B  SP+2..3: a_E,a_D
	; SP+4..5: ret  SP+6..9: b_orig

	; Copy b from original stack to locals
	; b_orig at SP+6..9: C2,B2,E2,D2
	ldhl	sp, #8		; → b_E
	ld	a, (hl)
	inc	hl		; → b_D
	ld	h, (hl)
	ld	l, a		; HL = b_D:b_E
	push	hl		; save b_DE at SP

	ldhl	sp, #8		; → b_C (stack grew 2)
	ld	a, (hl)
	inc	hl		; → b_B
	ld	h, (hl)
	ld	l, a		; HL = b_B:b_C
	push	hl		; save b_BC at SP

	; Now: SP+0..1: b_C,b_B  SP+2..3: b_E,b_D
	;      SP+4..5: a_C,a_B  SP+6..7: a_E,a_D
	;      SP+8..9: ret      SP+10..13: b_orig

	; === Step 1: a / b ===
	; Push b as arg2 for ___divsf3
	; Need to push high word (DE) first, then low (BC)
	ldhl	sp, #2		; → b_E
	ld	a, (hl)
	inc	hl		; → b_D
	ld	h, (hl)
	ld	l, a		; HL = b_D:b_E
	push	hl		; arg2 high

	ldhl	sp, #2		; → b_C (stack grew 2)
	ld	a, (hl)
	inc	hl		; → b_B
	ld	h, (hl)
	ld	l, a		; HL = b_B:b_C
	push	hl		; arg2 low

	; Reload a from saved location
	ldhl	sp, #8		; → a_C (4 arg2 + 4 saved_b)
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	inc	hl
	ld	e, (hl)
	inc	hl
	ld	d, (hl)		; DEBC = a

	call	___divsf3	; callee-cleanup 4 bytes, DEBC = a/b

	; === Step 2: trunc ===
	call	_truncf		; DEBC = trunc(a/b) = q

	; === Step 3: q * b ===
	; Push b as arg2
	ldhl	sp, #2
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, a
	push	hl		; b_DE high

	ldhl	sp, #2
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, a
	push	hl		; b_BC low

	; DEBC = q (from truncf, still valid)
	call	___mulsf3	; callee-cleanup, DEBC = q*b

	; === Step 4: a - q*b ===
	; Push q*b as arg2 (subtrahend)
	push	de		; q*b high
	push	bc		; q*b low

	; Reload a from saved
	ldhl	sp, #8		; → a_C (4 arg2 + 4 saved_b)
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	inc	hl
	ld	e, (hl)
	inc	hl
	ld	d, (hl)		; DEBC = a

	call	___subsf3	; callee-cleanup, DEBC = a - q*b = fmod

	; Clean up locals (8 bytes: saved_b + saved_a)
	add	sp, #8

	; Callee-cleanup original b (4 bytes)
	pop	hl		; return address
	add	sp, #4		; skip b_orig
	jp	(hl)
