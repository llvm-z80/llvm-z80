; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _fmaxf

;===------------------------------------------------------------------------===;
; _fmaxf - Return maximum of two floats
;
; Input:  HLDE = a, stack = b  (callee-cleanup for b)
; Output: HLDE = fmax(a, b)
;         If one is NaN, return the other.
;
; Uses ___cmpsf2 (callee-cleanup, returns DE: -1/0/+1).
;===------------------------------------------------------------------------===;
_fmaxf:
	push	ix
	ld	ix, #0
	add	ix, sp

	; Save a below IX
	push	hl		; IX-2: a_L, IX-1: a_H
	push	de		; IX-4: a_E, IX-3: a_D

	; --- NaN check: a ---
	ld	a, h
	and	#0x7F
	cp	#0x7F
	jr	nz, __fmaxf_a_ok
	bit	7, l
	jr	z, __fmaxf_a_ok
	ld	a, l
	and	#0x7F
	or	d
	or	e
	jr	nz, __fmaxf_ret_b	; a is NaN → return b
__fmaxf_a_ok:

	; --- NaN check: b ---
	ld	a, 7(ix)
	and	#0x7F
	cp	#0x7F
	jr	nz, __fmaxf_cmp
	bit	7, 6(ix)
	jr	z, __fmaxf_cmp
	ld	a, 6(ix)
	and	#0x7F
	or	5(ix)
	or	4(ix)
	jr	nz, __fmaxf_ret_a	; b is NaN → return a

__fmaxf_cmp:
	; Push b copy for cmpsf2
	ld	b, 7(ix)
	ld	c, 6(ix)
	push	bc
	ld	b, 5(ix)
	ld	c, 4(ix)
	push	bc
	; Reload a
	ld	h, -1(ix)
	ld	l, -2(ix)
	ld	d, -3(ix)
	ld	e, -4(ix)
	call	___cmpsf2	; callee-cleanup, DE = result
	; DE: -1 if a<b, 0 if a==b, +1 if a>b
	bit	7, d
	jr	z, __fmaxf_age	; a >= b (DE >= 0)
	; a < b → return b
	jr	__fmaxf_ret_b

__fmaxf_age:
	; a >= b: if a > b return a, if a == b return a (either is fine)
	jr	__fmaxf_ret_a

__fmaxf_ret_b:
	ld	h, 7(ix)
	ld	l, 6(ix)
	ld	d, 5(ix)
	ld	e, 4(ix)
	jr	__fmaxf_done

__fmaxf_ret_a:
	ld	h, -1(ix)
	ld	l, -2(ix)
	ld	d, -3(ix)
	ld	e, -4(ix)

__fmaxf_done:
	ld	sp, ix
	pop	ix
	pop	bc		; return address
	inc	sp
	inc	sp
	inc	sp
	inc	sp
	push	bc
	ret
