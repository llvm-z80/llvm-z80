; RUN: llc -mtriple=z80 -O1 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=sm83 -O1 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=z80 -O0 -verify-machineinstrs < %s -o /dev/null
; RUN: llc -mtriple=sm83 -O0 -verify-machineinstrs < %s -o /dev/null

; A return value too large for registers is stored through a hidden sret
; pointer taken from the stack. Functions with no formal arguments used to
; skip the code that reads that pointer and crashed in lowerReturn.

; The function must read the pointer from the incoming stack slot and store
; the value through it.
; CHECK-LABEL: ret_i64:
; CHECK: {{add hl,sp|ldhl sp}}
; CHECK: ld (hl
; CHECK: ret
define i64 @ret_i64() {
  ret i64 1234605616436508552 ; 0x1122334455667788
}

; CHECK-LABEL: ret_f64:
; CHECK: {{add hl,sp|ldhl sp}}
; CHECK: ld (hl
; CHECK: ret
define double @ret_f64() {
  ret double 1.5
}
