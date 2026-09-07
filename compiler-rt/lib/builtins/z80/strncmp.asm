; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _strncmp

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
	jr	z, _strncmp_eq	; n exhausted, strings equal so far
	ld	a, (de)		; A = *str2, so the compare can use (HL) directly
	cp	(hl)		; zero if equal, carry if *str2 < *str1
	jr	nz, _strncmp_diff
	or	a		; A is *str2, equal to *str1; zero ends both
	jr	z, _strncmp_eq
	inc	hl
	inc	de
	dec	bc
	jr	_strncmp_loop
_strncmp_eq:
	ld	de, #0
	jr	_strncmp_ret
_strncmp_diff:
	jr	c, _strncmp_greater	; *str2 < *str1, so str1 sorts after
	ld	de, #0xFFFF
	jr	_strncmp_ret
_strncmp_greater:
	ld	de, #1
_strncmp_ret:
	pop	ix
	pop	bc		; save return address
	inc	sp
	inc	sp		; callee-cleanup: skip 2 bytes of stack args
	push	bc
	ret
