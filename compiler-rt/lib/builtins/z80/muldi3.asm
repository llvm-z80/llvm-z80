; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___muldi3

;===------------------------------------------------------------------------===;
; ___muldi3 - 64-bit multiply (low 64 bits of result)
;
; Calling convention (sret demotion, SDCC __sdcccall(1)):
;   Stack: sret pointer (2B), a (8B), b (8B)
;
; Stack frame after push ix; ld ix,#0; add ix,sp:
;   IX+0,1       = saved IX
;   IX+2,3       = return address
;   IX+4,5       = sret pointer
;   IX+6,7       = a[0] (word 0, lowest)
;   IX+8,9       = a[1]
;   IX+10,11     = a[2]
;   IX+12,13     = a[3] (word 3, highest)
;   IX+14,15     = b[0]
;   IX+16,17     = b[1]
;   IX+18,19     = b[2]
;   IX+20,21     = b[3]
;
; Algorithm: Schoolbook multiply using 16-bit words.
;   result[0..3] = sum of a[i]*b[j] at position i+j (mod 2^64).
;   Uses __mulhi3 (low 16 of 16x16) and __umulhi3 (high 16 of 16x16).
;
; Local frame: IX-8..IX-1 = result[0..3] (4 x 16-bit words)
;===------------------------------------------------------------------------===;
___muldi3:
	push	ix
	ld	ix, #0
	add	ix, sp
	ld	hl, #-8
	add	hl, sp
	ld	sp, hl

	; Zero result
	xor	a
	ld	-8(ix), a
	ld	-7(ix), a
	ld	-6(ix), a
	ld	-5(ix), a
	ld	-4(ix), a
	ld	-3(ix), a
	ld	-2(ix), a
	ld	-1(ix), a

	;=== a[0] * b[0] → result[0], carry into result[1..3] ===
	ld	l, 6(ix)
	ld	h, 7(ix)
	ld	e, 14(ix)
	ld	d, 15(ix)
	call	___mulhi3	; DE = lo(a0*b0)
	ld	-8(ix), e
	ld	-7(ix), d	; result[0] = lo(a0*b0)

	ld	l, 6(ix)
	ld	h, 7(ix)
	ld	e, 14(ix)
	ld	d, 15(ix)
	call	___umulhi3	; DE = hi(a0*b0)
	ld	-6(ix), e
	ld	-5(ix), d	; result[1] = hi(a0*b0)

	;=== a[0] * b[1] → add to result[1], carry into result[2..3] ===
	ld	l, 6(ix)
	ld	h, 7(ix)
	ld	e, 16(ix)
	ld	d, 17(ix)
	call	___mulhi3
	ld	l, -6(ix)
	ld	h, -5(ix)
	add	hl, de
	ld	-6(ix), l
	ld	-5(ix), h
	jr	nc, _md_a0b1l_nc
	inc	-4(ix)
	jr	nz, _md_a0b1l_nc
	inc	-3(ix)
	jr	nz, _md_a0b1l_nc
	inc	-2(ix)
	jr	nz, _md_a0b1l_nc
	inc	-1(ix)
_md_a0b1l_nc:

	ld	l, 6(ix)
	ld	h, 7(ix)
	ld	e, 16(ix)
	ld	d, 17(ix)
	call	___umulhi3
	ld	l, -4(ix)
	ld	h, -3(ix)
	add	hl, de
	ld	-4(ix), l
	ld	-3(ix), h
	jr	nc, _md_a0b1h_nc
	inc	-2(ix)
	jr	nz, _md_a0b1h_nc
	inc	-1(ix)
_md_a0b1h_nc:

	;=== a[1] * b[0] → add to result[1], carry into result[2..3] ===
	ld	l, 8(ix)
	ld	h, 9(ix)
	ld	e, 14(ix)
	ld	d, 15(ix)
	call	___mulhi3
	ld	l, -6(ix)
	ld	h, -5(ix)
	add	hl, de
	ld	-6(ix), l
	ld	-5(ix), h
	jr	nc, _md_a1b0l_nc
	inc	-4(ix)
	jr	nz, _md_a1b0l_nc
	inc	-3(ix)
	jr	nz, _md_a1b0l_nc
	inc	-2(ix)
	jr	nz, _md_a1b0l_nc
	inc	-1(ix)
_md_a1b0l_nc:

	ld	l, 8(ix)
	ld	h, 9(ix)
	ld	e, 14(ix)
	ld	d, 15(ix)
	call	___umulhi3
	ld	l, -4(ix)
	ld	h, -3(ix)
	add	hl, de
	ld	-4(ix), l
	ld	-3(ix), h
	jr	nc, _md_a1b0h_nc
	inc	-2(ix)
	jr	nz, _md_a1b0h_nc
	inc	-1(ix)
_md_a1b0h_nc:

	;=== a[0] * b[2] → add to result[2], carry into result[3] ===
	ld	l, 6(ix)
	ld	h, 7(ix)
	ld	e, 18(ix)
	ld	d, 19(ix)
	call	___mulhi3
	ld	l, -4(ix)
	ld	h, -3(ix)
	add	hl, de
	ld	-4(ix), l
	ld	-3(ix), h
	jr	nc, _md_a0b2l_nc
	inc	-2(ix)
	jr	nz, _md_a0b2l_nc
	inc	-1(ix)
_md_a0b2l_nc:

	ld	l, 6(ix)
	ld	h, 7(ix)
	ld	e, 18(ix)
	ld	d, 19(ix)
	call	___umulhi3
	ld	l, -2(ix)
	ld	h, -1(ix)
	add	hl, de
	ld	-2(ix), l
	ld	-1(ix), h

	;=== a[1] * b[1] → add to result[2], carry into result[3] ===
	ld	l, 8(ix)
	ld	h, 9(ix)
	ld	e, 16(ix)
	ld	d, 17(ix)
	call	___mulhi3
	ld	l, -4(ix)
	ld	h, -3(ix)
	add	hl, de
	ld	-4(ix), l
	ld	-3(ix), h
	jr	nc, _md_a1b1l_nc
	inc	-2(ix)
	jr	nz, _md_a1b1l_nc
	inc	-1(ix)
_md_a1b1l_nc:

	ld	l, 8(ix)
	ld	h, 9(ix)
	ld	e, 16(ix)
	ld	d, 17(ix)
	call	___umulhi3
	ld	l, -2(ix)
	ld	h, -1(ix)
	add	hl, de
	ld	-2(ix), l
	ld	-1(ix), h

	;=== a[2] * b[0] → add to result[2], carry into result[3] ===
	ld	l, 10(ix)
	ld	h, 11(ix)
	ld	e, 14(ix)
	ld	d, 15(ix)
	call	___mulhi3
	ld	l, -4(ix)
	ld	h, -3(ix)
	add	hl, de
	ld	-4(ix), l
	ld	-3(ix), h
	jr	nc, _md_a2b0l_nc
	inc	-2(ix)
	jr	nz, _md_a2b0l_nc
	inc	-1(ix)
_md_a2b0l_nc:

	ld	l, 10(ix)
	ld	h, 11(ix)
	ld	e, 14(ix)
	ld	d, 15(ix)
	call	___umulhi3
	ld	l, -2(ix)
	ld	h, -1(ix)
	add	hl, de
	ld	-2(ix), l
	ld	-1(ix), h

	;=== result[3] only: a[0]*b[3] + a[1]*b[2] + a[2]*b[1] + a[3]*b[0] ===
	; (no need for umulhi3 — higher bits discarded)

	ld	l, 6(ix)
	ld	h, 7(ix)
	ld	e, 20(ix)
	ld	d, 21(ix)
	call	___mulhi3
	ld	l, -2(ix)
	ld	h, -1(ix)
	add	hl, de
	ld	-2(ix), l
	ld	-1(ix), h

	ld	l, 8(ix)
	ld	h, 9(ix)
	ld	e, 18(ix)
	ld	d, 19(ix)
	call	___mulhi3
	ld	l, -2(ix)
	ld	h, -1(ix)
	add	hl, de
	ld	-2(ix), l
	ld	-1(ix), h

	ld	l, 10(ix)
	ld	h, 11(ix)
	ld	e, 16(ix)
	ld	d, 17(ix)
	call	___mulhi3
	ld	l, -2(ix)
	ld	h, -1(ix)
	add	hl, de
	ld	-2(ix), l
	ld	-1(ix), h

	ld	l, 12(ix)
	ld	h, 13(ix)
	ld	e, 14(ix)
	ld	d, 15(ix)
	call	___mulhi3
	ld	l, -2(ix)
	ld	h, -1(ix)
	add	hl, de
	ld	-2(ix), l
	ld	-1(ix), h

	;=== Copy result to sret pointer ===
	ld	l, 4(ix)
	ld	h, 5(ix)
	ld	a, -8(ix)
	ld	(hl), a
	inc	hl
	ld	a, -7(ix)
	ld	(hl), a
	inc	hl
	ld	a, -6(ix)
	ld	(hl), a
	inc	hl
	ld	a, -5(ix)
	ld	(hl), a
	inc	hl
	ld	a, -4(ix)
	ld	(hl), a
	inc	hl
	ld	a, -3(ix)
	ld	(hl), a
	inc	hl
	ld	a, -2(ix)
	ld	(hl), a
	inc	hl
	ld	a, -1(ix)
	ld	(hl), a

	ld	sp, ix
	pop	ix
	ret
