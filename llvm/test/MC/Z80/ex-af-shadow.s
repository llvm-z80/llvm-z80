; RUN: llvm-mc -triple=z80 -show-encoding %s | FileCheck %s
; RUN: not llvm-mc -triple=sm83 %s 2>&1 | FileCheck %s --check-prefix=SM83
;
; The apostrophe of af' is not read as a character constant, and what follows
; it still parses. SM83 has no shadow registers.

; CHECK:      ex af,af' ; encoding: [0x08]
; CHECK-NEXT: ex af,af' ; encoding: [0x08]
; CHECK-NEXT: exx ; encoding: [0xd9]
; CHECK-NEXT: ld a,120 ; encoding: [0x3e,0x78]
; SM83: error: instruction requires a CPU feature not currently enabled
	ex	af,af'
	EX	AF, AF' ; comment
	exx
	ld	a,'x'
