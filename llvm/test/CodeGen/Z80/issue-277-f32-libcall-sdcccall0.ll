; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 -z80-float-sdcccall0 %s -o - | FileCheck %s
; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 %s -o - | FileCheck --check-prefix=DEFAULT %s
;
; Under the opt-in flag `-z80-float-sdcccall0`, the f32 arithmetic libcalls
; (__addsf3/__subsf3/__mulsf3/__divsf3) are emitted with
; CallingConv::Z80_SDCCCall0 instead of the default CallingConv::C.
;
; WHY: some float runtimes for this target (e.g. z88dk's math32) ship
; sdcccall(0)-ABI wrappers: ALL arguments pushed on the stack in declared
; order, 32-bit result returned in DE:HL (D=MSB). The flag lets a caller
; linking such a runtime alias these libcalls with zero glue code. It stays
; OFF by default because the standalone/ELF path's own compiler-rt float
; runtime expects the default C ABI (sdcccall(1)): first arg in a register
; pair, only the second arg on the stack.
;
; C source (compiled with -mllvm -z80-float-sdcccall0 to target the
; sdcccall(0) runtime):
;   float add(float a, float b) { return a + b; }
;   float sub(float a, float b) { return a - b; }
;   float mul(float a, float b) { return a * b; }
;   float div(float a, float b) { return a / b; }
; Without the flag: first arg stays in a register pair, only the second arg
; is pushed -- a caller targeting an sdcccall(0) runtime needed a word-swap
; shim to bridge this. With the flag: both args are pushed (4x PUSH HL).

define float @add(float %a, float %b) {
; CHECK-LABEL: _add:
; CHECK: push hl
; CHECK: push hl
; CHECK: push hl
; CHECK: push hl
; CHECK: call ___addsf3
;
; DEFAULT-LABEL: _add:
; DEFAULT: call ___addsf3
  %r = fadd float %a, %b
  ret float %r
}

define float @sub(float %a, float %b) {
; CHECK-LABEL: _sub:
; CHECK: push hl
; CHECK: push hl
; CHECK: push hl
; CHECK: push hl
; CHECK: call ___subsf3
  %r = fsub float %a, %b
  ret float %r
}

define float @mul(float %a, float %b) {
; CHECK-LABEL: _mul:
; CHECK: push hl
; CHECK: push hl
; CHECK: push hl
; CHECK: push hl
; CHECK: call ___mulsf3
  %r = fmul float %a, %b
  ret float %r
}

define float @div(float %a, float %b) {
; CHECK-LABEL: _div:
; CHECK: push hl
; CHECK: push hl
; CHECK: push hl
; CHECK: push hl
; CHECK: call ___divsf3
  %r = fdiv float %a, %b
  ret float %r
}
