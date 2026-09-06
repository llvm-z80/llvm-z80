; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _memchr

;===------------------------------------------------------------------------===;
; _memchr - Find byte in memory block
;
; Input:  HL = ptr, DE = byte (E = value), stack = size (i16)
; Output: DE = pointer to byte, or 0 if not found
;===------------------------------------------------------------------------===;

_memchr:
	push	ix
	ld	ix, #0
	add	ix, sp
	ld	c, 4(ix)	; BC = size
	ld	b, 5(ix)
_memchr_loop:
	ld	a, b
	or	c
	jr	z, _memchr_notfound
	ld	a, (hl)
	cp	e
	jr	z, _memchr_found
	inc	hl
	dec	bc
	jr	_memchr_loop
_memchr_found:
	ex	de, hl
	pop	ix
	pop	bc		; save return address
	inc	sp
	inc	sp		; callee-cleanup: skip 2 bytes of stack args
	push	bc
	ret
_memchr_notfound:
	ld	de, #0
	pop	ix
	pop	bc		; save return address
	inc	sp
	inc	sp		; callee-cleanup: skip 2 bytes of stack args
	push	bc
	ret
