	.module mos6502
	.globl ext, zext
	.area CODE
start:
	bne ext
	beq start2
	lda *zext
	lda ext
	jsr ext
	lda #<ext
	lda #>ext
	.area CODE2
start2:
	bne start
