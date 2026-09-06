; RUN: llc -mtriple=z80 -O1 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=sm83 -O1 -verify-machineinstrs < %s -o /dev/null
; RUN: llc -mtriple=z80 -O0 -verify-machineinstrs < %s -o /dev/null

; modf lowers to its libcall with a byte-aligned stack slot for the
; integral part; the generic path's size-aligned temporary would be
; rejected by the byte-aligned stack.

; CHECK-LABEL: frac:
; CHECK: call _modff
define float @frac(float %x) {
  %r = call { float, float } @llvm.modf.f32(float %x)
  %f = extractvalue { float, float } %r, 0
  ret float %f
}

; CHECK-LABEL: whole:
; CHECK: call _modff
define float @whole(float %x) {
  %r = call { float, float } @llvm.modf.f32(float %x)
  %i = extractvalue { float, float } %r, 1
  ret float %i
}
