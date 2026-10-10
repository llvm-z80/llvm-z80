; RUN: llvm-mc -triple=z80 -filetype=obj %s -o - | llvm-readelf -r - | FileCheck %s
; RUN: llvm-mc -triple=sm83 -filetype=obj --defsym=SM83=1 %s -o - | llvm-readelf -r - | FileCheck %s --check-prefix=SM83
; RUN: not llvm-mc -triple=z80 -filetype=obj --defsym=QUAD=1 %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=QUAD

; Relocations use the GNU binutils types. As in GNU as, a DJNZ addend holds the
; instruction-end adjustment, an LDH operand is a plain R_Z80_8, and an SM83 SP
; offset is an R_Z80_8_DIS.

.ifdef QUAD
	.quad	ext
.else
.ifdef SM83
	ldh	(hram), a
	ldh	a, (hram)
	add	sp, disp
	ldhl	sp, disp
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
; SM83-NEXT: 00000005 {{.*}} R_Z80_8_DIS {{.*}} disp + 0
; SM83-NEXT: 00000007 {{.*}} R_Z80_8_DIS {{.*}} disp + 0

; QUAD: error: 8-byte data relocations are not supported
