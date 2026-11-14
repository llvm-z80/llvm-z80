; RUN: llc -mtriple=z80 -O1 -z80-static-frames -verify-machineinstrs < %s | FileCheck %s

; Test that an external declaration with +no-callback does not treat the call
; as an escape to CallsExternalNode, allowing the caller to receive a static frame.

declare void @putchar(i8) #0

; CHECK-LABEL: main:
; CHECK: L_main.frame
define i16 @main() {
  %buf = alloca [4 x i16], align 1
  store volatile i16 42, ptr %buf, align 1
  call void @putchar(i8 65)
  ret i16 0
}

; CHECK: L___static_frames
; CHECK: L_main.frame = L___static_frames

attributes #0 = { "target-features"="+no-callback" }
