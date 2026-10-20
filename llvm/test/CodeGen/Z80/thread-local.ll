; RUN: llc -verify-machineinstrs -mtriple=z80 < %s | FileCheck %s --check-prefix=Z80 \
; RUN:   --implicit-check-not=.tdata --implicit-check-not=.tbss
; RUN: llc -verify-machineinstrs -mtriple=sm83 < %s | FileCheck %s --check-prefix=SM83 \
; RUN:   --implicit-check-not=.tdata --implicit-check-not=.tbss
; RUN: llc -verify-machineinstrs -mtriple=z80 -z80-asm-format=sdasz80 < %s \
; RUN:   | FileCheck %s --check-prefix=SDAS
; RUN: llc -verify-machineinstrs -mtriple=z80 -z80-asm-format=z88dk < %s \
; RUN:   | FileCheck %s --check-prefix=Z88DK
; RUN: llc -verify-machineinstrs -mtriple=z80 -emulated-tls < %s \
; RUN:   | FileCheck %s --check-prefix=EMU --implicit-check-not='{{^}}_counter:' \
; RUN:     --implicit-check-not='{{^}}_zero:'

; There is no thread pointer, so a thread-local variable is an ordinary global.
; Under emulated TLS each access calls __emutls_get_address instead.

@counter = internal thread_local global i16 3
@zero = thread_local global i16 0

define i16 @bump() {
; Z80-LABEL: _bump:
; Z80:         ld de,(_counter)
; Z80-NEXT:    inc de
; Z80-NEXT:    ld (_counter),de
;
; SM83-LABEL: _bump:
; SM83:        ld hl,_counter
;
; SDAS-LABEL: _bump:
; SDAS:         ld de,(_counter)
;
; Z88DK-LABEL: _bump:
; Z88DK:        ld de,(_counter)
;
; EMU-LABEL: _bump:
; EMU:         ld hl,___emutls_v.counter
; EMU-NEXT:    call ___emutls_get_address
  %p = call ptr @llvm.threadlocal.address.p0(ptr @counter)
  %v = load i16, ptr %p
  %inc = add i16 %v, 1
  store i16 %inc, ptr %p
  ret i16 %inc
}

define i16 @read_zero() {
; Z80-LABEL: _read_zero:
; Z80:         ld de,(_zero)
;
; SM83-LABEL: _read_zero:
; SM83:        ld hl,_zero
;
; SDAS-LABEL: _read_zero:
; SDAS:         ld de,(_zero)
;
; Z88DK-LABEL: _read_zero:
; Z88DK:        ld de,(_zero)
;
; EMU-LABEL: _read_zero:
; EMU:         ld hl,___emutls_v.zero
; EMU-NEXT:    call ___emutls_get_address
  %p = call ptr @llvm.threadlocal.address.p0(ptr @zero)
  %v = load i16, ptr %p
  ret i16 %v
}

; Z80:         .data
; Z80-NEXT:  _counter:
; Z80:         .bss
; Z80-NEXT:    .globl _zero
; Z80-NEXT:  _zero:

; SM83:        .data
; SM83-NEXT: _counter:
; SM83:        .bss
; SM83-NEXT:   .globl _zero
; SM83-NEXT: _zero:

; SDAS:        .area _DATA
; SDAS-NEXT: _counter:
; SDAS:        .area _BSS
; SDAS-NEXT:   .globl _zero
; SDAS-NEXT: _zero:

; Z88DK:       SECTION data_compiler
; Z88DK-NEXT: _counter:
; Z88DK:       SECTION bss_compiler
; Z88DK-NEXT:  GLOBAL _zero
; Z88DK-NEXT: _zero:

; EMU:       ___emutls_v.counter:
; EMU:       ___emutls_t.counter:
; EMU-NEXT:    .short 3
; EMU:       ___emutls_v.zero:
