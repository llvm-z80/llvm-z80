	.module hc08
	.globl ext, zext
	.area CODE
start:
	bra ext
	bne ext
	lda *zext
	sta *zext+1
	brset #0, *zext, ext
	bset #1, *zext
	lda ext
	jsr ext
	ldx #<ext
	ldx #>ext
	.area DIRECT (PAG)
dp:	.ds 2
	.area CODE
	.setdp 0, DIRECT
	lda *dp
