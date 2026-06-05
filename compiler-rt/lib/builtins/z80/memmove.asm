; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _memmove
	.globl _memmove_fwd
	.globl _memmove_ret

;===------------------------------------------------------------------------===;
; _memmove - Copy memory block (handles overlapping regions)
;
; Input:  HL = dest, DE = src, stack = size (i16)
; Output: DE = dest (original)
; If dest < src: forward copy (LDIR)
; If dest > src: backward copy (LDDR)
; If dest == src: no-op
;===------------------------------------------------------------------------===;
_memmove:
	push	ix
	ld	ix, #0
	add	ix, sp
	ld	c, 4(ix)	; BC = size
	ld	b, 5(ix)
	ld	a, b
	or	c
	jr	z, _memmove_ret	; size == 0
	; Compare dest(HL) vs src(DE)
	push	hl
	or	a		; clear carry
	sbc	hl, de
	pop	hl
	jr	z, _memmove_ret	; dest == src, no-op
	jr	c, _memmove_fwd	; dest < src, forward safe
	; dest > src: backward copy using LDDR
	; Point HL and DE to last bytes
	push	hl		; save original dest
	ex	de, hl		; HL = src, DE = dest
	add	hl, bc
	dec	hl		; HL = src + size - 1
	ex	de, hl		; DE = src + size - 1
	pop	hl		; HL = dest
	push	hl		; re-save dest for return
	add	hl, bc
	dec	hl		; HL = dest + size - 1
	ex	de, hl		; HL = src_end, DE = dest_end (LDDR format)
	lddr
	pop	de		; DE = original dest
	pop	ix
	pop	bc		; save return address
	inc	sp
	inc	sp		; callee-cleanup: skip 2 bytes of stack args
	push	bc
	ret
_memmove_fwd:
	; Forward copy using LDIR
	push	hl		; save original dest
	ex	de, hl		; HL = src, DE = dest (LDIR format)
	ldir
	pop	de		; DE = original dest
	pop	ix
	pop	bc		; save return address
	inc	sp
	inc	sp		; callee-cleanup: skip 2 bytes of stack args
	push	bc
	ret
_memmove_ret:
	ex	de, hl		; DE = dest (return value)
	pop	ix
	pop	bc		; save return address
	inc	sp
	inc	sp		; callee-cleanup: skip 2 bytes of stack args
	push	bc
	ret
