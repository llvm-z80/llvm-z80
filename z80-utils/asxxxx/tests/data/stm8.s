	.module stm8
	.globl ext
	.area CODE
start:
	jrne ext
	jra ext
	callf ext
	jpf ext
	ldf a, ext
	ld a, ext
	call ext
	ld a, #<ext
	ld a, #>ext
	btjt ext, #3, start
