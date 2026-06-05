; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___floatsisf
	.globl __flsi_nz
	.globl __flsi_pos
	.globl __flsi_norm_h
	.globl __flsi_nlp
	.globl __flsi_normed
	.globl __flsi_rndup
	.globl __flsi_nornd
	.globl __flsi_e0
	.globl ___floatunsisf

;   6. Pack sign + exponent + mantissa
;===------------------------------------------------------------------------===;
___floatsisf:
	; Check for zero
	ld	a, h
	or	l
	or	d
	or	e
	jr	nz, __flsi_nz
	; Return +0.0
	ret			; HLDE already 0

__flsi_nz:
	; Save sign
	ld	a, h
	and	#0x80
	push	af		; sign on stack

	; Take absolute value if negative
	bit	7, h
	jr	z, __flsi_pos
	; Negate HLDE
	ld	a, e
	cpl
	ld	e, a
	ld	a, d
	cpl
	ld	d, a
	ld	a, l
	cpl
	ld	l, a
	ld	a, h
	cpl
	ld	h, a
	inc	e
	jr	nz, __flsi_pos
	inc	d
	jr	nz, __flsi_pos
	inc	l
	jr	nz, __flsi_pos
	inc	h
__flsi_pos:
	; H:L:D:E = |value| (positive 32-bit)
	; Normalize: shift left until H[7] is set, then pack H:L:D as 24-bit mantissa.
	; E becomes round info. Exponent starts at 158 (=127+31) and decrements per shift.
	; Skip zero high bytes for speed.
	ld	b, #158		; B = exponent

	; First skip zero bytes for speed
	ld	a, h
	or	a
	jr	nz, __flsi_norm_h
	; H=0, shift left 8
	ld	h, l
	ld	l, d
	ld	d, e
	ld	e, #0
	ld	a, b
	sub	#8
	ld	b, a
	ld	a, h
	or	a
	jr	nz, __flsi_norm_h
	; Still 0, shift another 8
	ld	h, l
	ld	l, d
	ld	d, #0
	ld	a, b
	sub	#8
	ld	b, a
	ld	a, h
	or	a
	jr	nz, __flsi_norm_h
	; Another 8
	ld	h, l
	ld	l, #0
	ld	a, b
	sub	#8
	ld	b, a

__flsi_norm_h:
	; H != 0 (or we shifted enough). Shift left until H[7] is set.
__flsi_nlp:
	bit	7, h
	jr	nz, __flsi_normed
	sla	e
	rl	d
	rl	l
	rl	h
	dec	b		; exponent--
	jr	__flsi_nlp

__flsi_normed:
	; H[7] is set. Mantissa = H:L:D (24 bits), E = round info.
	; Round bit = E[7], Sticky = E[6:0] != 0, Guard = D[0]

	; Round-to-nearest-even
	bit	7, e		; R (round bit)
	jr	z, __flsi_nornd
	; R=1: check sticky for tie-breaking
	ld	a, e
	and	#0x7F		; sticky bits
	jr	nz, __flsi_rndup	; S!=0 → round up
	; S=0, R=1: tie. Round to even (check G=D[0])
	bit	0, d
	jr	z, __flsi_nornd	; G=0 → round down (even)
__flsi_rndup:
	; Add 1 to mantissa H:L:D (the 24-bit part)
	inc	d
	jr	nz, __flsi_nornd
	inc	l
	jr	nz, __flsi_nornd
	inc	h
	jr	nz, __flsi_nornd
	; Overflow: H was 0xFF, became 0x00
	; This means mantissa overflowed → set H=0x80, exponent++
	ld	h, #0x80
	inc	b		; exponent++
__flsi_nornd:
	; Now pack: sign(1) + exponent(8) + mantissa[22:0](23)
	; Mantissa is in H:L:D with H[7] = implicit bit.
	; We return in HLDE format:
	;   result_H = sign | (exp >> 1)
	;   result_L = (exp[0] << 7) | H[6:0]   (mantissa[22:16] from H)
	;   result_D = L                          (mantissa[15:8])
	;   result_E = D                          (mantissa[7:0])
	;
	; Pack: rearrange H:L:D → result L:D:E, encode sign + exponent
	ld	e, d		; result_E = mantissa[7:0]
	ld	d, l		; result_D = mantissa[15:8]
	ld	a, h
	and	#0x7F		; remove implicit bit
	bit	0, b
	jr	z, __flsi_e0
	or	#0x80		; set exp[0]
__flsi_e0:
	ld	l, a
	; result_H = sign | (exp >> 1)
	ld	a, b
	srl	a		; A = exp >> 1
	ld	c, a		; C = exp >> 1
	pop	af		; A = sign byte (0x00 or 0x80), from initial push
	and	#0x80
	or	c		; A = sign | (exp >> 1)
	ld	h, a
	ret

;===------------------------------------------------------------------------===;
; ___floatunsisf - Convert unsigned int32 to float
;
; Input:  HLDE = unsigned int32
; Output: HLDE = float
;
; Same as ___floatsisf but no sign handling. Always positive.
;===------------------------------------------------------------------------===;
___floatunsisf:
	; Check for zero
	ld	a, h
	or	l
	or	d
	or	e
	ret	z		; return +0.0

	; Push sign = 0 (positive) for shared pack code
	xor	a
	push	af
	; Fall through to __flsi_pos (shared normalize/round/pack)
	jr	__flsi_pos
