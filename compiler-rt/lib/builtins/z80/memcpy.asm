; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _memcpy
	.globl ___memcpy
	.globl ___z80_memcpy_builtin

;===------------------------------------------------------------------------===;
; ___z80_memcpy_builtin - Copy memory block (CallingConv::Z80_Builtin)
;
; Input:  HL = dest, DE = src, BC = size
; Output: none
;
; LDIR copies (HL)->(DE), the opposite of the C argument order, and decrements
; BC before testing it, so a zero size would copy 65536 bytes.
;===------------------------------------------------------------------------===;
___z80_memcpy_builtin:
	ex	de, hl		; HL = src, DE = dest (LDIR format)
	ld	a, b
	or	c
	ret	z		; size == 0
	ldir
	ret

;===------------------------------------------------------------------------===;
; _memcpy - Copy memory block, C entry point
;
; Input:  HL = dest, DE = src, stack = size (i16)
; Output: DE = dest (original)
;
; SDCC lowers a struct assignment to __memcpy and keeps both names in one
; library module, so defining only one of them pulls in SDCC's memcpy too and
; the link fails on the duplicate.
;===------------------------------------------------------------------------===;
_memcpy:
___memcpy:
	push	ix
	ld	ix, #0
	add	ix, sp
	ld	c, 4(ix)	; BC = size (3rd arg from stack)
	ld	b, 5(ix)
	push	hl		; save dest for return value
	call	___z80_memcpy_builtin
	pop	de		; DE = original dest (return value)
	pop	ix
	pop	bc		; save return address
	inc	sp
	inc	sp		; callee-cleanup: skip 2 bytes of stack args
	push	bc		; re-push return address
	ret
