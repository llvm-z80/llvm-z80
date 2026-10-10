; RUN: llc -mtriple=sm83 -O1 < %s | FileCheck %s
; RUN: llc -mtriple=sm83 -O1 -z80-asm-format=sdasz80 < %s | FileCheck %s --check-prefix=SDAS
; RUN: llc -mtriple=sm83 -O1 < %s | llvm-mc -triple=sm83 -filetype=obj -o /dev/null

; A negative SP offset is printed as one, so the output assembles again.

declare void @g(ptr)

define void @f() {
; CHECK-LABEL: f:
; CHECK:       add sp,-6
; CHECK:       add sp,6
; SDAS-LABEL:  f:
; SDAS:        add sp,#-6
; SDAS:        add sp,#6
  %a = alloca [6 x i8]
  call void @g(ptr %a)
  ret void
}
