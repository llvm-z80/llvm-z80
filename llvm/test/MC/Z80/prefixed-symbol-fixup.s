; RUN: llvm-mc -triple=z80 -filetype=obj %s -o %t.o
; RUN: llvm-readelf -r %t.o | FileCheck %s

; A symbol operand's fixup must sit after every opcode byte. ED/DD/FD
; prefixed loads carry two opcode bytes, so their address fixup belongs at
; offset 2; it used to be placed at offset 1, where the linker then
; overwrote the second opcode byte with the symbol's low address byte.

	ld	(_x),de		; ED 53 nn nn — fixup at +2
	ld	(_x),bc		; ED 43 nn nn
	ld	(_x),sp		; ED 73 nn nn
	ld	de,(_x)		; ED 5B nn nn
	ld	(_x),hl		; 22 nn nn — one opcode byte, fixup at +1
	ld	ix,_x		; DD 21 nn nn — fixup at +2
	in	a,(_p)		; DB nn — a port operand fixup belongs at +1,
	out	(_p),a		; D3 nn — not on the opcode byte

; The instructions start at 0, 4, 8, 0xc, 0x10, 0x13, 0x17, 0x19.
; CHECK: 00000002 {{.*}} R_Z80_16 {{.*}} _x
; CHECK: 00000006 {{.*}} R_Z80_16 {{.*}} _x
; CHECK: 0000000a {{.*}} R_Z80_16 {{.*}} _x
; CHECK: 0000000e {{.*}} R_Z80_16 {{.*}} _x
; CHECK: 00000011 {{.*}} R_Z80_16 {{.*}} _x
; CHECK: 00000015 {{.*}} R_Z80_16 {{.*}} _x
; CHECK: 00000018 {{.*}} R_Z80_8 {{.*}} _p
; CHECK: 0000001a {{.*}} R_Z80_8 {{.*}} _p
