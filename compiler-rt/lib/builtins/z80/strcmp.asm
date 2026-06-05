; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _strcmp
	.globl _strcmp_done

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
	sub	b		; A = *str1 - *str2
	jr	nz, _strcmp_done
	ld	a, (hl)
	or	a		; both equal and null?
	jr	z, _strcmp_done
	inc	hl
	inc	de
	jr	_strcmp
_strcmp_done:
	ld	e, a		; sign-extend A into DE
	ld	d, #0
	bit	7, a
	ret	z
	ld	d, #0xFF
	ret
