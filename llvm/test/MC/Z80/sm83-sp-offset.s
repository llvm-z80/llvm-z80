; RUN: llvm-mc -triple=sm83 -show-encoding %s | FileCheck %s
; RUN: not llvm-mc -triple=sm83 --defsym=ERR=1 %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERR

; The CPU sign-extends the offset of ADD SP,e and LDHL SP,e, so 128 to 255
; are rejected instead of being taken as negative offsets.

.ifdef ERR
; ERR: [[#@LINE+1]]:{{[0-9]+}}: error: immediate operand out of range
	add	sp, 128
; ERR: [[#@LINE+1]]:{{[0-9]+}}: error: immediate operand out of range
	add	sp, 255
; ERR: [[#@LINE+1]]:{{[0-9]+}}: error: immediate operand out of range
	ldhl	sp, -129
.else
; CHECK: add sp,-128 ; encoding: [0xe8,0x80]
	add	sp, -128
; CHECK: add sp,127 ; encoding: [0xe8,0x7f]
	add	sp, 127
; CHECK: ldhl sp,-1 ; encoding: [0xf8,0xff]
	ldhl	sp, -1
.endif
