	.module rabbit
	.globl ext
	.area _CODE
start:
	jr ext
	djnz ext
	ld hl, #ext
	ld a, #<ext
	ld a, #>ext
	ld hl, 4(sp)
