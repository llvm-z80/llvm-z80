; RUN: llvm-mc -triple=z80 -filetype=obj %s -o - | llvm-readelf -r - | FileCheck %s
; RUN: llvm-mc -triple=sm83 -filetype=obj --defsym=SM83=1 %s -o - | llvm-readelf -r - | FileCheck %s --check-prefix=SM83
; RUN: not llvm-mc -triple=z80 -filetype=obj --defsym=QUAD=1 %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=QUAD

; Relocations use the GNU binutils types. Like GNU as, a DJNZ relocation
; carries the instruction-end adjustment in its addend, and an LDH operand is
; an ordinary 8-bit relocation against the high-page address.

.ifdef QUAD
	.quad	ext
.else
.ifdef SM83
	ldh	(hram), a
	ldh	a, (hram)
.else
	ld	a, ext
	ld	hl, ext
	in	a, (port)
	ld	a, (ix+disp)
	djnz	ext
	djnz	ext+5
	.byte	ext
	.short	ext
	.long	ext
	.byte	ext@z80_16lo
	.byte	ext@z80_16hi
.endif
.endif

; CHECK:      00000001 {{.*}} R_Z80_8       {{.*}} ext + 0
; CHECK-NEXT: 00000003 {{.*}} R_Z80_16      {{.*}} ext + 0
; CHECK-NEXT: 00000006 {{.*}} R_Z80_8       {{.*}} port + 0
; CHECK-NEXT: 00000009 {{.*}} R_Z80_8_DIS   {{.*}} disp + 0
; CHECK-NEXT: 0000000b {{.*}} R_Z80_8_PCREL {{.*}} ext - 1
; CHECK-NEXT: 0000000d {{.*}} R_Z80_8_PCREL {{.*}} ext + 4
; CHECK-NEXT: 0000000e {{.*}} R_Z80_8       {{.*}} ext + 0
; CHECK-NEXT: 0000000f {{.*}} R_Z80_16      {{.*}} ext + 0
; CHECK-NEXT: 00000011 {{.*}} R_Z80_32      {{.*}} ext + 0
; CHECK-NEXT: 00000015 {{.*}} R_Z80_BYTE0   {{.*}} ext + 0
; CHECK-NEXT: 00000016 {{.*}} R_Z80_BYTE1   {{.*}} ext + 0

; SM83:      00000001 {{.*}} R_Z80_8 {{.*}} hram + 0
; SM83-NEXT: 00000003 {{.*}} R_Z80_8 {{.*}} hram + 0

; QUAD: error: 8-byte data relocations are not supported
