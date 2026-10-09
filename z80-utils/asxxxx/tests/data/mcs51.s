	.module mcs51
	.globl ext, extbit
	.area CSEG (CODE)
start:
	ajmp ext
	acall ext+0x10
	sjmp ext
	jz ext
	cjne a, #3, ext
	djnz r0, ext
	setb extbit
	jb extbit, start
	mov c, extbit
	mov a, #<ext
	mov a, #>ext
	ljmp ext
	.area CSEG2 (CODE)
other:
	sjmp start
	ajmp start
	.area BSEG_BYTES (DATA)
flags:	.ds 2
	.area CSEG4 (CODE)
	setb flags[3]
	clr flags[6]
	jb flags[1], .
	mov c, flags[7]
