; RUN: llc -mtriple=z80 -O1 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=z80 -O0 -verify-machineinstrs < %s -o /dev/null

; There is no instruction cache, so the intrinsic dissolves into nothing.

; CHECK-LABEL: flush:
; CHECK-NOT: call
; CHECK: ret
define void @flush(ptr %b, ptr %e) {
  call void @llvm.clear_cache(ptr %b, ptr %e)
  ret void
}
