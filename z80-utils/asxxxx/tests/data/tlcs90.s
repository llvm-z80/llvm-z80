	.module tlcs90
	.globl ext
	.area _CODE
start:
	jr ext
	djnz ext
	ld hl, #ext
	ld a, #<ext
