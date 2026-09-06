; RUN: llc -mtriple=z80 -O1 -z80-static-frames -verify-machineinstrs < %s | FileCheck %s

; An indirect call inside an interrupt handler may reach any address-taken
; or externally-callable function, so every one of them counts as reachable
; from the handler's context as well and keeps its stack frame.

@sink = global ptr null

; CHECK-LABEL: pointed:
; CHECK-NOT: L_pointed.frame
define internal i16 @pointed(i16 %x) {
  %buf = alloca i16, align 1
  store volatile i16 %x, ptr %buf, align 1
  %v = load volatile i16, ptr %buf, align 1
  ret i16 %v
}

; CHECK-LABEL: isr:
; CHECK-NOT: L_isr.frame
define void @isr() #0 {
  %fp = load ptr, ptr @sink, align 1
  %r = call i16 %fp(i16 1)
  ret void
}

; CHECK-LABEL: main:
; CHECK-NOT: L_main.frame
define i16 @main() {
  %buf = alloca [4 x i16], align 1
  store volatile i16 1, ptr %buf, align 1
  store ptr @pointed, ptr @sink, align 1
  %r = call i16 @pointed(i16 2)
  ret i16 %r
}

attributes #0 = { "interrupt" }
