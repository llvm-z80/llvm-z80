; RUN: llc -mtriple=z80 -O1 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=sm83 -O1 -verify-machineinstrs < %s -o /dev/null
; RUN: llc -mtriple=z80 -O0 -verify-machineinstrs < %s -o /dev/null

; The va_list is a single pointer, so va_copy is a pointer load and store.

; CHECK-LABEL: copy:
; CHECK: ret
define void @copy(i16 %n, ...) {
  %ap = alloca ptr, align 1
  %ap2 = alloca ptr, align 1
  call void @llvm.va_start(ptr %ap)
  call void @llvm.va_copy(ptr %ap2, ptr %ap)
  call void @llvm.va_end(ptr %ap)
  call void @llvm.va_end(ptr %ap2)
  ret void
}

declare void @llvm.va_start(ptr)
declare void @llvm.va_copy(ptr, ptr)
declare void @llvm.va_end(ptr)
