; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _memcpy
	.globl _memcpy_done

;===------------------------------------------------------------------------===;
; _memcpy - Copy memory block
;
; Input:  HL = dest, DE = src, stack = size (i16)
; Output: DE = dest (original)
; Uses LDIR: copies (HL)->(DE), HL++, DE++, BC--, repeat until BC=0
; Note: LDIR source is HL, dest is DE, so we swap HL/DE from calling conv.
;===------------------------------------------------------------------------===;
_memcpy:
	push	ix
	ld	ix, #0
	add	ix, sp
	ld	c, 4(ix)	; BC = size (3rd arg from stack)
	ld	b, 5(ix)
	push	hl		; save dest for return value
	ex	de, hl		; HL = src, DE = dest (LDIR format)
	ld	a, b
	or	c
	jr	z, _memcpy_done
	ldir
_memcpy_done:
	pop	de		; DE = original dest (return value)
	pop	ix
	pop	bc		; save return address
	inc	sp
	inc	sp		; callee-cleanup: skip 2 bytes of stack args
	push	bc		; re-push return address
	ret
