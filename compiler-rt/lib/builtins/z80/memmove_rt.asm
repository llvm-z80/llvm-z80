; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___memmove_rt

;===------------------------------------------------------------------------===;
; __memmove_rt - register-CC (z80_allreg) memmove helper.
;
; ABI: CallingConv::Z80_AllReg.  Arguments arrive entirely in registers:
;   HL = dest, DE = src, BC = size (i16).  No stack argument, no IX frame,
;   no callee-cleanup -- this is what makes it far smaller than the public
;   stack-ABI _memmove.  Called only by the G_MEMMOVE
;   lowering (Z80LegalizerInfo) for the runtime-unknown-direction case; the
;   public string.h memmove (_memmove) keeps the standard C ABI.
;
; Nothing is callee-saved under z80_allreg, so HL/DE/BC/A/FLAGS are free to
; clobber.  The lowering ignores the return value (G_MEMMOVE is void), so we
; do not bother restoring dest.
;
;   dest < src : forward copy  (LDIR)
;   dest > src : backward copy (LDDR)
;   dest == src or size == 0 : no-op
;===------------------------------------------------------------------------===;
___memmove_rt:
	ld	a, b
	or	c
	ret	z		; size == 0 -> nothing to do (also avoids LDIR/LDDR BC=0 = 65536)
	; compare dest (HL) vs src (DE)
	push	hl
	or	a		; clear carry
	sbc	hl, de
	pop	hl
	ret	z		; dest == src, no-op
	jr	c, .Lmemmove_rt_fwd	; dest < src -> forward
	; dest > src: backward copy via LDDR.
	; Need HL = src+size-1, DE = dest+size-1, BC = size.
	add	hl, bc
	dec	hl		; HL = dest + size - 1
	ex	de, hl		; HL = src, DE = dest + size - 1
	add	hl, bc
	dec	hl		; HL = src + size - 1
	lddr
	ret
.Lmemmove_rt_fwd:
	ex	de, hl		; HL = src, DE = dest (LDIR format)
	ldir
	ret
