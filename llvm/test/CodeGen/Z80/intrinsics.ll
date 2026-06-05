; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O0 < %s | FileCheck %s

declare void @llvm.z80.halt()
declare void @llvm.z80.di()
declare void @llvm.z80.ei()
declare void @llvm.z80.nop()

; Test: HALT intrinsic
define void @test_halt() {
; CHECK-LABEL: _test_halt:
; CHECK:       halt
; CHECK:       ret
  call void @llvm.z80.halt()
  ret void
}

; Test: DI and EI intrinsics for interrupt control
define void @test_di_ei() {
; CHECK-LABEL: _test_di_ei:
; CHECK:       di{{$}}
; CHECK-NEXT:  ei
; CHECK-NEXT:  ret
  call void @llvm.z80.di()
  call void @llvm.z80.ei()
  ret void
}

; Test: NOP intrinsic
define void @test_nop() {
; CHECK-LABEL: _test_nop:
; CHECK:       nop
; CHECK:       ret
  call void @llvm.z80.nop()
  ret void
}
