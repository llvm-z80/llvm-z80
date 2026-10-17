	.module ds390
	.DS80C390
	.amode 2
	.globl ext
	.area CSEG (CODE)
start:
	ljmp ext
	lcall ext+0x10000
	mov dptr, #ext
	sjmp ext
	.db <ext, >ext, ext>>16
	.dw ext
	.area BSEG_BYTES (DATA)
flags:	.ds 2
	.area CSEG4 (CODE)
	setb flags[3]
	jb flags[1], .
