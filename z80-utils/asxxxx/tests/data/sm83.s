	.module sm83
	.globl _hr
	.area _CODE
	ldh (_hr), a
	ldh a, (_hr+1)
	ld a, (_hr)
	jr _hr
