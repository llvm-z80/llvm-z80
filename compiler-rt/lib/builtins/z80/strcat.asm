; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _strcat
	.globl _strcat_find
	.globl _strcat_copy
	.globl _strcat_done

_strcat:
	push	hl		; save dest for return
	; Find end of dest
	xor	a
_strcat_find:
	cp	(hl)
	inc	hl
	jr	nz, _strcat_find
	dec	hl		; HL = null terminator of dest
	; Copy src to end of dest
	ex	de, hl		; HL = src, DE = end of dest
_strcat_copy:
	ld	a, (hl)
	ld	(de), a
	or	a
	jr	z, _strcat_done
	inc	hl
	inc	de
	jr	_strcat_copy
_strcat_done:
	pop	de		; DE = original dest
	ret

;===------------------------------------------------------------------------===;
; _strchr - Find character in string
;
; Input:  HL = string, DE = character (E = char)
; Output: DE = pointer to char, or 0 if not found
