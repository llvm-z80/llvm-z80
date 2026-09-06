; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___floatdisf
	.globl ___floatundisf
	.globl ___floattisf
	.globl ___floatuntisf

;===------------------------------------------------------------------------===;
; ___floatdisf / ___floatundisf / ___floattisf / ___floatuntisf  (SM83)
;   Convert a 64-bit or 128-bit integer to float
;
; Input:  the integer on the stack at SP+2, little-endian (callee cleans)
; Output: DEBC = float
;
; Same shape as the Z80 version: narrow the value to its top thirty-two
; significant bits, fold "something was dropped" into the low bit of that
; window, hand it to ___floatunsisf for the rounding, and scale the answer by
; the power of two the narrowing stood for.
;
; Four bytes of locals sit below the return address, since there is no index
; register to hold them:
;   SP+0 sign   SP+1 sticky   SP+2 index, then scale   SP+3 byte count
; which puts the return address at SP+4 and the argument at SP+6.
;===------------------------------------------------------------------------===;

___floatdisf:
	ld	a, #8
	jr	__flt_signed_entry

___floattisf:
	ld	a, #16
	jr	__flt_signed_entry

___floatundisf:
	ld	a, #8
	jr	__flt_unsigned_entry

___floatuntisf:
	ld	a, #16

__flt_unsigned_entry:
	add	sp, #-4
	ldhl	sp, #3
	ld	(hl), a		; byte count
	ldhl	sp, #0
	ld	(hl), #0	; positive
	jr	__flt_scan_start

__flt_signed_entry:
	add	sp, #-4
	ldhl	sp, #3
	ld	(hl), a		; byte count
	ldhl	sp, #0
	ld	(hl), #0	; positive until the top byte says otherwise
	dec	a
	ldhl	sp, #6
	add	a, l
	ld	l, a
	ld	a, #0
	adc	a, h
	ld	h, a		; HL = the last argument byte
	bit	7, (hl)
	jr	z, __flt_scan_start
	ld	a, #0x80
	ldhl	sp, #0
	ld	(hl), a
	; Negate the argument where it lies: it is the caller's own copy, which
	; the calling convention makes dead the moment the call returns. That
	; rests on the call frame not being reserved, so each call site pushes
	; the argument afresh rather than writing it into a slot it reuses.
	ldhl	sp, #3
	ld	b, (hl)
	ldhl	sp, #6
	xor	a		; clears the carry as well
__flt_neg_lp:
	ld	a, #0
	sbc	a, (hl)
	ld	(hl+), a
	dec	b
	jr	nz, __flt_neg_lp

__flt_scan_start:
	; Walk down from the top for the highest byte that is not zero.
	ldhl	sp, #3
	ld	a, (hl)
	ld	b, a		; B = byte count
	dec	a
	ld	c, a		; C = index of the byte at HL
	ldhl	sp, #6
	add	a, l
	ld	l, a
	ld	a, #0
	adc	a, h
	ld	h, a
__flt_scan:
	ld	a, (hl)
	or	a
	jr	nz, __flt_found
	dec	hl
	dec	c
	dec	b
	jr	nz, __flt_scan
	; Every byte was zero.
	ld	d, #0
	ld	e, #0
	ld	b, #0
	ld	c, #0
	jp	__flt_ret

__flt_found:
	ld	a, c
	cp	#4
	jr	c, __flt_narrow

	; Everything below the five-byte window only contributes a sticky bit.
	ld	d, #0
	sub	#4
	jr	z, __flt_sticky_done
	ld	b, a
	ldhl	sp, #6
__flt_sticky_lp:
	ld	a, (hl)
	or	d
	ld	d, a
	inc	hl
	dec	b
	jr	nz, __flt_sticky_lp
__flt_sticky_done:
	ldhl	sp, #1
	ld	(hl), d		; sticky
	ldhl	sp, #2
	ld	(hl), c		; index

	; HL = the highest non-zero byte
	ld	a, c
	ldhl	sp, #6
	add	a, l
	ld	l, a
	ld	a, #0
	adc	a, h
	ld	h, a
	; Window, most significant first: D E B C, with L holding the fifth byte.
	ld	d, (hl)
	dec	hl
	ld	e, (hl)
	dec	hl
	ld	b, (hl)
	dec	hl
	ld	c, (hl)
	dec	hl
	ld	a, (hl)
	ld	l, a
	ld	h, #0		; H = shift count
	bit	7, d
	jr	nz, __flt_norm_done
__flt_norm_lp:
	sla	l
	rl	c
	rl	b
	rl	e
	rl	d
	inc	h
	bit	7, d
	jr	z, __flt_norm_lp
__flt_norm_done:
	; The shift count and the leftover byte have nowhere to live but the
	; stack, which moves the locals down two while they are there.
	push	hl
	ldhl	sp, #3		; sticky
	ld	a, (hl)
	ldhl	sp, #0		; the leftover byte
	or	(hl)
	jr	z, __flt_no_sticky
	set	0, c
__flt_no_sticky:
	; The window stands for 2^(index*8 - 24 - shift) times its own value.
	ldhl	sp, #4		; index
	ld	a, (hl)
	add	a, a
	add	a, a
	add	a, a
	sub	#24
	ldhl	sp, #1		; the shift count
	sub	(hl)
	ldhl	sp, #4
	ld	(hl), a		; scale, where the index was
	pop	hl
	call	___floatunsisf
	; Scaling by a power of two is an addition to the exponent, which starts
	; eight bits up in the high half. The shift below reads the scale as
	; unsigned, which holds because this path needs an index of at least
	; four, putting the scale between 1 and 96.
	ldhl	sp, #2
	ld	a, (hl)
	rrca
	ld	h, a
	and	#0x80
	ld	l, a
	ld	a, h
	and	#0x7F
	ld	h, a		; HL = scale * 128
	ld	a, e
	add	a, l
	ld	e, a
	ld	a, d
	adc	a, h
	ld	d, a
	jr	__flt_ret

__flt_narrow:
	; The value fits in the low four bytes, so it converts directly.
	ldhl	sp, #6
	ld	a, (hl+)
	ld	c, a
	ld	a, (hl+)
	ld	b, a
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)
	call	___floatunsisf

__flt_ret:
	ldhl	sp, #0
	ld	a, (hl)
	or	d
	ld	d, a		; apply the sign
	ldhl	sp, #3
	ld	a, (hl)		; byte count, which survives what follows
	add	sp, #4
	pop	hl		; return address
	cp	#8
	jr	z, __flt_ret8
	add	sp, #16
	jp	(hl)
__flt_ret8:
	add	sp, #8
	jp	(hl)
