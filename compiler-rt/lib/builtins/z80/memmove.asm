; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _memmove
	.globl ___z80_memmove_builtin

;===------------------------------------------------------------------------===;
; ___z80_memmove_builtin - Copy memory block (CallingConv::Z80_Builtin)
;
; Input:  HL = dest, DE = src, BC = size
; Output: none
;
;   dest < src : forward copy  (LDIR)
;   dest > src : backward copy (LDDR)
;   dest == src or size == 0 : nothing to do
;
; Both decrement BC before testing it, so a zero size would copy 65536 bytes.
;===------------------------------------------------------------------------===;
___z80_memmove_builtin:
	ld	a, b
	or	c
	ret	z		; size == 0
	push	hl		; compare dest (HL) against src (DE)
	or	a		; clear carry
	sbc	hl, de
	pop	hl
	ret	z		; dest == src
	jr	c, ___z80_memmove_fwd	; dest < src -> ascending is safe
	; dest > src: copy descending from the last byte of each region.
	add	hl, bc
	dec	hl		; HL = dest + size - 1
	ex	de, hl		; HL = src, DE = dest + size - 1
	add	hl, bc
	dec	hl		; HL = src + size - 1
	lddr
	ret
___z80_memmove_fwd:
	ex	de, hl		; HL = src, DE = dest (LDIR format)
	ldir
	ret

;===------------------------------------------------------------------------===;
; _memmove - Copy memory block, C entry point
;
; Input:  HL = dest, DE = src, stack = size (i16)
; Output: DE = dest (original)
;===------------------------------------------------------------------------===;
_memmove:
	push	ix
	ld	ix, #0
	add	ix, sp
	ld	c, 4(ix)	; BC = size
	ld	b, 5(ix)
	push	hl		; save dest for return value
	call	___z80_memmove_builtin
	pop	de		; DE = original dest (return value)
	pop	ix
	pop	bc		; save return address
	inc	sp
	inc	sp		; callee-cleanup: skip 2 bytes of stack args
	push	bc		; re-push return address
	ret
