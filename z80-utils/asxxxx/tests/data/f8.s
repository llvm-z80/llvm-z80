	.module f8
	.globl ext
	.area CODE
start:
	jr ext
	jrnz ext
	ld xl, #<ext
	ld xl, #>ext
