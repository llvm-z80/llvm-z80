; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _memset
	.globl _memset_done

;===------------------------------------------------------------------------===;
; _memset - Fill memory block
;
; Input:  HL = dest, DE = value (E = byte), stack = size (i16)
; Output: DE = dest (original)
; Writes first byte, then uses LDIR to propagate to remaining bytes.
;===------------------------------------------------------------------------===;
_memset:
	push	ix
	ld	ix, #0
	add	ix, sp
	ld	c, 4(ix)	; BC = size
	ld	b, 5(ix)
	push	hl		; save dest for return value
	ld	a, b
	or	c
	jr	z, _memset_done
	ld	(hl), e		; write first byte
	dec	bc		; remaining = size - 1
	ld	a, b
	or	c
	jr	z, _memset_done	; size was 1
	ld	d, h		; DE = HL (points to first byte)
	ld	e, l
	inc	de		; DE = HL + 1 (next byte)
	ldir			; copy first byte to remaining
_memset_done:
	pop	de		; DE = original dest (return value)
	pop	ix
	pop	bc		; save return address
	inc	sp
	inc	sp		; callee-cleanup: skip 2 bytes of stack args
	push	bc
	ret
