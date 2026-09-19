; RUN: llc -mtriple=z80 -O1 -z80-static-frames -verify-machineinstrs < %s | FileCheck %s

; Inline asm cannot transfer control to another function: it has no callee
; at all, unlike an indirect call through a function pointer. An interrupt
; handler that only contains inline asm (the ubiquitous ei/di sequence every
; Z80 ISR needs) must not be treated as if it could reach every
; externally-callable function in the module; a leaf function with no
; relationship to the handler keeps its static frame.

; CHECK-LABEL: isr:
define void @isr() #0 {
  call void asm sideeffect "ei", ""()
  ret void
}

; CHECK-LABEL: compare_6bytes:
; CHECK: L_compare_6bytes.frame
define internal i16 @compare_6bytes(i16 %x) {
  %buf = alloca [4 x i16], align 1
  store volatile i16 %x, ptr %buf, align 1
  %v = load volatile i16, ptr %buf, align 1
  ret i16 %v
}

; CHECK-LABEL: user:
define void @user(i16 %x) {
  %r = call i16 @compare_6bytes(i16 %x)
  ret void
}

attributes #0 = { "interrupt" }
