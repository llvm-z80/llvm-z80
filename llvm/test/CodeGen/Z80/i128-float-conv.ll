; RUN: llc -mtriple=z80 -O1 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=sm83 -O1 -verify-machineinstrs < %s -o /dev/null

; 128-bit integer endpoints go through their conversion libcalls, following
; the same reference-compiles policy as the double-precision routines.

; CHECK-LABEL: to_i128:
; CHECK: call ___fixsfti
define i128 @to_i128(float %x) {
  %r = fptosi float %x to i128
  ret i128 %r
}

; CHECK-LABEL: to_u128:
; CHECK: call ___fixunssfti
define i128 @to_u128(float %x) {
  %r = fptoui float %x to i128
  ret i128 %r
}

; CHECK-LABEL: from_i128:
; CHECK: call ___floattisf
define float @from_i128(i128 %x) {
  %r = sitofp i128 %x to float
  ret float %r
}
