; RUN: llc -mtriple=z80 -O1 -z80-static-frames -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=sm83 -O1 -z80-static-frames -verify-machineinstrs < %s | FileCheck %s

; Inline asm and the Z80 intrinsics never call into the module. An interrupt
; handler using them does not reach main, and main keeps its static frame.

; CHECK-LABEL: isr:
; CHECK-NOT: L_isr.frame
define void @isr() #0 {
  %buf = alloca [2 x i8], align 1
  store volatile i8 1, ptr %buf, align 1
  call void asm sideeffect "ei", ""()
  call void @llvm.z80.ei()
  ret void
}

define internal void @leaf_asm() {
  %buf = alloca [8 x i8], align 1
  store volatile i8 2, ptr %buf, align 1
  call void asm sideeffect "nop", ""()
  ret void
}

define internal void @leaf_intrinsic() {
  %buf = alloca [8 x i8], align 1
  store volatile i8 3, ptr %buf, align 1
  call void @llvm.z80.di()
  ret void
}

; CHECK-LABEL: main:
; CHECK: L_main.frame
define i16 @main() {
  %buf = alloca [4 x i8], align 1
  store volatile i8 4, ptr %buf, align 1
  call void @leaf_asm()
  call void @leaf_intrinsic()
  ret i16 0
}

; The leaves never run at the same time. Their frames overlap.
; CHECK-DAG: L_main.frame = L___static_frames{{$}}
; CHECK-DAG: L_leaf_asm.frame = L___static_frames+4{{$}}
; CHECK-DAG: L_leaf_intrinsic.frame = L___static_frames+4{{$}}

declare void @llvm.z80.ei()
declare void @llvm.z80.di()

attributes #0 = { "interrupt" }
