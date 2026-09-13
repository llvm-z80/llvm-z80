; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _strcmp

;===------------------------------------------------------------------------===;
; _strcmp - Compare two null-terminated strings
;
; Input:  HL = str1, DE = str2
; Output: DE = negative/zero/positive (str1 <=> str2)
;===------------------------------------------------------------------------===;
_strcmp:
	ld	a, (de)
	ld	b, a		; B = *str2
	ld	a, (hl)		; A = *str1
	cp	b		; zero if equal, carry if *str1 < *str2
	jr	nz, _strcmp_diff
	or	a		; A is still *str1; zero means both ended
	jr	z, _strcmp_eq
	inc	hl
	inc	de
	jr	_strcmp
_strcmp_eq:
	ld	de, #0
	ret
_strcmp_diff:
	jr	c, _strcmp_less
	ld	de, #1
	ret
_strcmp_less:
	ld	de, #0xFFFF
	ret
