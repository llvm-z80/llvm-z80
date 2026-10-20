; REQUIRES: z80
; RUN: llvm-mc -filetype=obj -triple=sm83 %s -o %t.o
; RUN: ld.lld %t.o --defsym=hram=0xff80 -o /dev/null 2>&1 | count 0
; RUN: not ld.lld %t.o --defsym=hram=0x80 -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERR
; RUN: llvm-objcopy --strip-symbol='$x' --strip-symbol='$d' %t.o %t.nomap.o
; RUN: ld.lld %t.nomap.o --defsym=hram=0x80 -o /dev/null 2>&1 | FileCheck %s --check-prefix=WARN
; RUN: llvm-mc -filetype=obj -triple=z80 --defsym=Z80=1 %s -o %t.z80.o
; RUN: ld.lld %t.z80.o --defsym=hram=0x80 -o /dev/null 2>&1 | count 0

; An LDH operand must be a high-page address. The data byte after 0xF0 is not
; an LDH operand, which only the mapping symbols can tell; without them the
; linker warns about both. Z80 code is not checked.

	.globl	_start
_start:
.ifdef Z80
	ld	(ix-16), hram
.else
	ldh	a, (hram)
	ret
	.byte	0xf0, hram
.endif

; ERR:     error: {{.*}}LDH address 0x80 is not in [0xff00, 0xffff]; references 'hram'
; ERR-NOT: LDH address

; WARN:      warning: {{.*}}possible LDH address 0x80 is not in [0xff00, 0xffff]; references 'hram'
; WARN-NEXT: warning: {{.*}}possible LDH address 0x80 is not in [0xff00, 0xffff]; references 'hram'
