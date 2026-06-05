; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s

declare void @llvm.memcpy.p0.p0.i16(ptr nocapture writeonly, ptr nocapture readonly, i16, i1 immarg)
declare void @llvm.memmove.p0.p0.i16(ptr nocapture, ptr nocapture readonly, i16, i1 immarg)
declare void @llvm.memset.p0.i16(ptr nocapture writeonly, i8, i16, i1 immarg)

; Test: memcpy lowers to runtime call with correct args
define void @test_memcpy(ptr %dst, ptr %src, i16 %n) {
; CHECK-LABEL: _test_memcpy:
; CHECK:       call _memcpy
  call void @llvm.memcpy.p0.p0.i16(ptr %dst, ptr %src, i16 %n, i1 false)
  ret void
}

; Test: memmove lowers to runtime call with correct args
define void @test_memmove(ptr %dst, ptr %src, i16 %n) {
; CHECK-LABEL: _test_memmove:
; CHECK:       call _memmove
  call void @llvm.memmove.p0.p0.i16(ptr %dst, ptr %src, i16 %n, i1 false)
  ret void
}

; Test: memset lowers to runtime call with i8 val promoted to i16
define void @test_memset(ptr %dst, i8 %val, i16 %n) {
; CHECK-LABEL: _test_memset:
; CHECK:       call _memset
  call void @llvm.memset.p0.i16(ptr %dst, i8 %val, i16 %n, i1 false)
  ret void
}

; Test: memset val is zero-extended (not sign-extended) to i16
define void @test_memset_zext(ptr %dst, i8 %val, i16 %n) {
; CHECK-LABEL: _test_memset_zext:
; CHECK:       ld {{[a-z]}},#0
  call void @llvm.memset.p0.i16(ptr %dst, i8 %val, i16 %n, i1 false)
  ret void
}
