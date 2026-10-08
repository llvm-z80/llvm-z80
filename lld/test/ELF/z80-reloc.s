; REQUIRES: z80
; RUN: llvm-mc -filetype=obj -triple=z80 %s -o %t.o
; RUN: ld.lld %t.o --defsym=ext=0x105 --defsym=disp=-3 --defsym=hram=0xff80 \
; RUN:   -Ttext=0x100 --section-start=.data=0x200 -o %t
; RUN: llvm-objdump -d %t | FileCheck %s
; RUN: llvm-objdump -s -j .data %t | FileCheck %s --check-prefix=DATA
; RUN: not ld.lld %t.o --defsym=ext=0x105 --defsym=disp=-3 --defsym=hram=0xc000 \
; RUN:   -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERR

; A PC-relative value is S + A - P; the DJNZ addend already holds the
; instruction-end adjustment. R_Z80_8 takes a high-page address like binutils.

	.globl	_start
_start:
	djnz	ext
	djnz	ext+5
	ld	a, (ix+disp)
	ld	a, hram
	ld	hl, ext

	.data
	.byte	ext@z80_16lo, ext@z80_16hi

; CHECK:      100: 10 03       djnz $105
; CHECK-NEXT: 102: 10 06       djnz $10a
; CHECK-NEXT: 104: dd 7e fd    ld a,(ix+-$3)
; CHECK-NEXT: 107: 3e 80       ld a,$80
; CHECK-NEXT: 109: 21 05 01    ld hl,$105

; DATA: 0200 0501

; ERR: relocation R_Z80_8 out of range: 49152 is not in [-256, 255]; references 'hram'
