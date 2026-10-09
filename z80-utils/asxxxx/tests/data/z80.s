	.module z80
	.globl ext
	.area _CODE
start:
	jr ext
	djnz ext
	jr nz, other
	ld a, #<(ext+300)
	ld a, #>(ext+300)
	ld hl, #ext-1
	.area _OTHER
other:
	jr start
	jp start
