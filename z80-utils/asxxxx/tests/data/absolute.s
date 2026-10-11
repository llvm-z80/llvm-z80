	.module absolute
	.globl port
port = 0x40
	.area _CODE
	ld a,(port)
	ld hl,#port
	.area _DATA
v::	.ds 2
	.area _CODE
	ld (v),hl
