; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _strncmp
	.globl _strncmp_loop
	.globl _strncmp_eq
	.globl _strncmp_done
	.globl _strncmp_ret

;===------------------------------------------------------------------------===;
; _strncmp - Compare two strings up to n bytes
;
; Input:  HL = str1, DE = str2, stack = n (i16)
; Output: DE = negative/zero/positive
;===------------------------------------------------------------------------===;
_strncmp:
	push	ix
	ld	ix, #0
	add	ix, sp
	ld	c, 4(ix)	; BC = n
	ld	b, 5(ix)
_strncmp_loop:
	ld	a, b
	or	c
	jr	z, _strncmp_eq	; n exhausted, strings equal
	ld	a, (de)
	ld	4(ix), a	; temp = *str2 (reuse stack slot)
	ld	a, (hl)		; A = *str1
	sub	4(ix)		; A = *str1 - *str2
	jr	nz, _strncmp_done
	ld	a, (hl)
	or	a
	jr	z, _strncmp_eq	; both null
	inc	hl
	inc	de
	dec	bc
	jr	_strncmp_loop
_strncmp_eq:
	xor	a
_strncmp_done:
	ld	e, a
	ld	d, #0
	bit	7, a
	jr	z, _strncmp_ret
	ld	d, #0xFF
_strncmp_ret:
	pop	ix
	pop	bc		; save return address
	inc	sp
	inc	sp		; callee-cleanup: skip 2 bytes of stack args
	push	bc
	ret
