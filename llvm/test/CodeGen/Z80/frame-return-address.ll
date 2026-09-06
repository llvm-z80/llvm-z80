; RUN: llc -mtriple=z80 -O1 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=sm83 -O1 -verify-machineinstrs < %s -o /dev/null
; RUN: llc -mtriple=z80 -O0 -verify-machineinstrs < %s -o /dev/null

; Both intrinsics resolve through a fixed stack object at offset zero: its
; address is the frame base, and its contents are the pushed return address.
; Deeper levels have no frame chain to walk and yield null.

; CHECK-LABEL: frame:
define ptr @frame() {
  %a = call ptr @llvm.frameaddress.p0(i32 0)
  ret ptr %a
}

; CHECK-LABEL: retaddr:
define ptr @retaddr() {
  %a = call ptr @llvm.returnaddress(i32 0)
  ret ptr %a
}

; CHECK-LABEL: deep:
; CHECK: ld de,0
define ptr @deep() {
  %a = call ptr @llvm.returnaddress(i32 2)
  ret ptr %a
}
