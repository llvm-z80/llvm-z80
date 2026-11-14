; RUN: llc -mtriple=z80 -O1 -z80-static-frames -mattr=+no-callbacks -verify-machineinstrs < %s | FileCheck %s

; Test that +no-callbacks assumes external calls do not re-enter the module,
; allowing main to receive a static frame despite calling external functions.

declare void @putchar(i8)

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
