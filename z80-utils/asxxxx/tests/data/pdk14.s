	.module pdk14
	.globl ext
	.area CODE
start:
	goto ext
	call ext+4
	mov a, ext
	mov ext, a
	set1 ext, #3
	ret #<ext
	ret #>ext
