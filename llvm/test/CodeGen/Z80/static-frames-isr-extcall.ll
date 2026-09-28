; RUN: llc -mtriple=z80 -O1 -z80-static-frames -verify-machineinstrs < %s | FileCheck %s

; A call to an external declaration inside an interrupt handler must not
; cause the handler's context to bleed into unrelated module functions
; whose addresses are not taken.

declare void @ext_helper()

; CHECK-LABEL: isr:
define void @isr() #0 {
  call void @ext_helper()
  ret void
}

; CHECK-LABEL: mainline_leaf:
; CHECK: L_mainline_leaf.frame
define internal i16 @mainline_leaf(i16 %x) {
  %buf = alloca [4 x i16], align 1
  store volatile i16 %x, ptr %buf, align 1
  %v = load volatile i16, ptr %buf, align 1
  ret i16 %v
}

; CHECK-LABEL: mainline_user:
; CHECK-NOT: add hl, sp
define void @mainline_user(i16 %x) {
  %r = call i16 @mainline_leaf(i16 %x)
  ret void
}

attributes #0 = { "interrupt" }
