; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _fmodf

;===------------------------------------------------------------------------===;
; _fmodf - Floating-point remainder: a - trunc(a/b) * b
;
; Input:  HLDE = a, stack = b  (callee-cleanup for b)
; Output: HLDE = fmod(a, b)
;
; Algorithm:
;   q = a / b           (___divsf3)
;   q = trunc(q)         (_truncf)
;   q = q * b            (___mulsf3)
;   result = a - q       (___subsf3)
;
; All binary float ops do callee-cleanup (remove 4 bytes of stack args).
;===------------------------------------------------------------------------===;
_fmodf:
	push	ix
	ld	ix, #0
	add	ix, sp
	; IX+0: saved IX, IX+2: ret addr, IX+4..7: b (E2,D2,L2,H2)

	; Save a (HLDE) below IX
	push	hl		; IX-2..IX-1: a_L, a_H
	push	de		; IX-4..IX-3: a_E, a_D

	; Step 1: a / b → push b as arg2, a in HLDE
	ld	b, 7(ix)
	ld	c, 6(ix)
	push	bc		; b H2:L2
	ld	b, 5(ix)
	ld	c, 4(ix)
	push	bc		; b D2:E2
	; Reload a
	ld	h, -1(ix)
	ld	l, -2(ix)
	ld	d, -3(ix)
	ld	e, -4(ix)
	call	___divsf3	; callee-cleanup, HLDE = a/b

	; Step 2: trunc
	call	_truncf		; HLDE = trunc(a/b) = q

	; Step 3: q * b → push b as arg2, q in HLDE
	ld	b, 7(ix)
	ld	c, 6(ix)
	push	bc
	ld	b, 5(ix)
	ld	c, 4(ix)
	push	bc
	; HLDE = q (from truncf)
	call	___mulsf3	; callee-cleanup, HLDE = q*b

	; Step 4: a - q*b → push q*b as arg2, load a as arg1
	push	hl		; q*b H:L
	push	de		; q*b D:E
	; Reload a
	ld	h, -1(ix)
	ld	l, -2(ix)
	ld	d, -3(ix)
	ld	e, -4(ix)
	call	___subsf3	; callee-cleanup, HLDE = a - q*b

	; Clean up saved a (4 bytes)
	ld	sp, ix
	pop	ix
	; Callee-cleanup: skip original b (4 bytes)
	pop	bc		; return address
	inc	sp
	inc	sp
	inc	sp
	inc	sp
	push	bc
	ret
