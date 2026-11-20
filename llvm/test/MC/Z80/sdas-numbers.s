; RUN: llvm-mc -triple=z80-unknown-sdcc -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=z80 -show-encoding --defsym=ELF=1 %s | FileCheck %s --check-prefix=ELF

; ELF: encoding: [0x3e,0x08]
; CHECK: encoding: [0x3e,0x0a]
	ld	a, #010
.ifndef ELF
; CHECK: encoding: [0x3e,0x6c]
	ld	a, #00108
; CHECK: encoding: [0x21,0x34,0x12]
	ld	hl, #0x1234
; CHECK: encoding: [0x3e,0x05]
	ld	a, #0b101
; CHECK: encoding: [0x3e,0x00]
	ld	a, #0
; CHECK: .db 10
	.byte	010
.endif
