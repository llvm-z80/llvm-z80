; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___floatdisf
	.globl ___floatundisf
	.globl ___floattisf
	.globl ___floatuntisf

;===------------------------------------------------------------------------===;
; ___floatdisf / ___floatundisf / ___floattisf / ___floatuntisf
;   Convert a 64-bit or 128-bit integer to float
;
; Input:  the integer on the stack at SP+2, little-endian (caller cleans)
; Output: HLDE = float
;
; The rounding is left to ___floatunsisf rather than repeated here. Only the
; top thirty-two significant bits can affect the result, so this narrows the
; wide value to exactly those and remembers whether anything was dropped:
;
;   - find the highest byte that is not zero, and normalize a five-byte window
;     ending there until its top bit is set, which puts the top 32 bits in the
;     upper four bytes and leaves the rest in the fifth
;   - fold "something was dropped" into the low bit of the window, which sits
;     below the round bit and so cannot disturb it
;   - convert the window, then scale by the power of two the shift stood for,
;     which is an addition to the exponent field
;
; A value below 2^32 skips all of that and is converted directly. A negative
; one is negated where it lies first: the argument is the caller's own copy,
; which the calling convention makes dead the moment the call returns. That
; rests on the call frame not being reserved, so each call site pushes the
; argument afresh rather than writing it into a slot it reuses.
;===------------------------------------------------------------------------===;

___floatdisf:
	push	ix
	ld	ix, #0
	add	ix, sp
	ld	c, #8
	jr	__flt_signed

___floattisf:
	push	ix
	ld	ix, #0
	add	ix, sp
	ld	c, #16
	jr	__flt_signed

___floatundisf:
	push	ix
	ld	ix, #0
	add	ix, sp
	ld	c, #8
	jr	__flt_unsigned

___floatuntisf:
	push	ix
	ld	ix, #0
	add	ix, sp
	ld	c, #16

__flt_unsigned:
	xor	a
	push	af		; sign byte at IX-1
	jr	__flt_scan_start

__flt_signed:
	call	__flt_top_ptr
	bit	7, (hl)
	jr	z, __flt_unsigned
	call	__flt_base_ptr
	ld	b, c
	or	a		; clear carry
__flt_neg_lp:
	ld	a, #0
	sbc	a, (hl)
	ld	(hl), a
	inc	hl
	djnz	__flt_neg_lp
	ld	a, #0x80
	push	af		; sign byte at IX-1

__flt_scan_start:
	; Walk down from the top for the highest byte that is not zero.
	call	__flt_top_ptr
	ld	b, c		; B = byte count
	dec	c		; C = index of the byte at HL
__flt_scan:
	ld	a, (hl)
	or	a
	jr	nz, __flt_found
	dec	hl
	dec	c
	djnz	__flt_scan
	; Every byte was zero.
	ld	hl, #0
	ld	d, h
	ld	e, l
	jr	__flt_sign

__flt_found:
	; C = index of the highest non-zero byte, HL points at it.
	ld	a, c
	cp	#4
	jr	c, __flt_narrow

	; Everything below the five-byte window only contributes a sticky bit.
	sub	#4
	ld	d, #0
	or	a
	jr	z, __flt_wide
	ld	b, a
	push	hl
	call	__flt_base_ptr
__flt_sticky_lp:
	ld	a, (hl)
	or	d
	ld	d, a
	inc	hl
	djnz	__flt_sticky_lp
	pop	hl

__flt_wide:
	push	de		; sticky at IX-3
	push	bc		; index at IX-6
	; Window, most significant first: B C D E, with L holding the fifth byte.
	ld	b, (hl)
	dec	hl
	ld	c, (hl)
	dec	hl
	ld	d, (hl)
	dec	hl
	ld	e, (hl)
	dec	hl
	ld	a, (hl)
	ld	l, a
	ld	h, #0		; H = shift count
	bit	7, b
	jr	nz, __flt_norm_done
__flt_norm_lp:
	sla	l
	rl	e
	rl	d
	rl	c
	rl	b
	inc	h
	bit	7, b
	jr	z, __flt_norm_lp
__flt_norm_done:
	; What is left in the fifth byte was dropped, as was anything below it.
	ld	a, l
	or	-3(ix)
	jr	z, __flt_no_sticky
	set	0, e
__flt_no_sticky:
	; The window stands for 2^(index*8 - 24 - shift) times its own value.
	ld	a, -6(ix)
	add	a, a
	add	a, a
	add	a, a
	sub	#24
	sub	h
	ld	-6(ix), a
	ld	h, b
	ld	l, c
	call	___floatunsisf
	; Scaling by a power of two is an addition to the exponent, which starts
	; eight bits up in the high half. The shift below reads the scale as
	; unsigned, which holds because this path needs an index of at least
	; four, putting the scale between 1 and 96.
	ld	a, -6(ix)
	ld	b, a
	ld	c, #0
	srl	b
	rr	c		; BC = scale * 128
	add	hl, bc
	jr	__flt_sign

__flt_narrow:
	; The value fits in the low four bytes, so it converts directly.
	call	__flt_base_ptr
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	inc	hl
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, a
	call	___floatunsisf

__flt_sign:
	ld	a, -1(ix)
	or	h
	ld	h, a
	ld	sp, ix
	pop	ix
	ret

; HL = the first argument byte.
__flt_base_ptr:
	push	ix
	pop	hl
	inc	hl
	inc	hl
	inc	hl
	inc	hl
	ret

; HL = the last argument byte, C holding the byte count.
__flt_top_ptr:
	call	__flt_base_ptr
	push	bc
	ld	b, #0
	dec	bc
	add	hl, bc
	pop	bc
	ret
