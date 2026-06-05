; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _fminf

;===------------------------------------------------------------------------===;
; _fminf - Return minimum of two floats
;
; Input:  HLDE = a, stack = b  (callee-cleanup for b)
; Output: HLDE = fmin(a, b)
;         If one is NaN, return the other.
;
; Uses ___cmpsf2 for comparison (callee-cleanup, returns DE: -1/0/+1).
;===------------------------------------------------------------------------===;
_fminf:
	push	ix
	ld	ix, #0
	add	ix, sp
	; IX+0: saved IX, IX+2: ret addr, IX+4..7: b (E2,D2,L2,H2)

	; Save a below IX
	push	hl		; IX-2: a_L, IX-1: a_H
	push	de		; IX-4: a_E, IX-3: a_D

	; --- NaN check: a ---
	ld	a, h
	and	#0x7F
	cp	#0x7F
	jr	nz, __fminf_a_ok
	bit	7, l
	jr	z, __fminf_a_ok
	ld	a, l
	and	#0x7F
	or	d
	or	e
	jr	nz, __fminf_ret_b	; a is NaN → return b
__fminf_a_ok:

	; --- NaN check: b ---
	ld	a, 7(ix)
	and	#0x7F
	cp	#0x7F
	jr	nz, __fminf_cmp
	bit	7, 6(ix)
	jr	z, __fminf_cmp
	ld	a, 6(ix)
	and	#0x7F
	or	5(ix)
	or	4(ix)
	jr	nz, __fminf_ret_a	; b is NaN → return a

__fminf_cmp:
	; Push b copy for cmpsf2
	ld	b, 7(ix)
	ld	c, 6(ix)
	push	bc		; H2:L2
	ld	b, 5(ix)
	ld	c, 4(ix)
	push	bc		; D2:E2
	; Reload a into HLDE
	ld	h, -1(ix)
	ld	l, -2(ix)
	ld	d, -3(ix)
	ld	e, -4(ix)
	call	___cmpsf2	; callee-cleanup removes 4 bytes, DE = result
	; DE: -1 if a<b, 0 if a==b, +1 if a>b
	bit	7, d
	jr	nz, __fminf_ret_a	; a < b → return a
	; a >= b → return b

__fminf_ret_b:
	ld	h, 7(ix)
	ld	l, 6(ix)
	ld	d, 5(ix)
	ld	e, 4(ix)
	jr	__fminf_done

__fminf_ret_a:
	ld	h, -1(ix)
	ld	l, -2(ix)
	ld	d, -3(ix)
	ld	e, -4(ix)

__fminf_done:
	ld	sp, ix		; discard saved a
	pop	ix
	; Callee-cleanup: skip 4 bytes of b
	pop	bc		; return address
	inc	sp
	inc	sp
	inc	sp
	inc	sp
	push	bc
	ret
