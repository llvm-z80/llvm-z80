; RUN: llc -mtriple=sm83 -O2 -z80-static-frames -verify-machineinstrs < %s | FileCheck %s

; On SM83 a wide slot costs a byte per access more in static memory than on the
; stack. Past the four bytes of the stack pointer adjustment the wide slots stay
; on the stack and the eight-bit slots stay static, with or without optsize.

; CHECK-LABEL: busy:
; CHECK: add sp,-6
; CHECK: ld (L_busy.frame),a
; CHECK: add sp,6
define internal i16 @busy(i16 %x, i8 %y) noinline {
  %w = alloca i16
  %b = alloca i8
  store volatile i16 %x, ptr %w
  store volatile i8 %y, ptr %b
  %1 = load volatile i16, ptr %w
  %2 = load volatile i16, ptr %w
  %3 = load volatile i16, ptr %w
  %4 = load volatile i16, ptr %w
  %c = load volatile i8, ptr %b
  %e = zext i8 %c to i16
  %s1 = add i16 %1, %2
  %s2 = add i16 %3, %4
  %s3 = add i16 %s1, %s2
  %s4 = add i16 %s3, %e
  ret i16 %s4
}

; CHECK-LABEL: busy_size:
; CHECK: add sp,-6
; CHECK: ld (L_busy_size.frame),a
; CHECK: add sp,6
define internal i16 @busy_size(i16 %x, i8 %y) noinline optsize {
  %w = alloca i16
  %b = alloca i8
  store volatile i16 %x, ptr %w
  store volatile i8 %y, ptr %b
  %1 = load volatile i16, ptr %w
  %2 = load volatile i16, ptr %w
  %3 = load volatile i16, ptr %w
  %4 = load volatile i16, ptr %w
  %c = load volatile i8, ptr %b
  %e = zext i8 %c to i16
  %s1 = add i16 %1, %2
  %s2 = add i16 %3, %4
  %s3 = add i16 %s1, %s2
  %s4 = add i16 %s3, %e
  ret i16 %s4
}

; CHECK-LABEL: quiet:
; CHECK-NOT: add sp
; CHECK: L_quiet.frame
; CHECK: ret
define internal i16 @quiet(i16 %x) noinline {
  %w = alloca i16
  store volatile i16 %x, ptr %w
  %1 = load volatile i16, ptr %w
  ret i16 %1
}

define i16 @main() {
  %a = call i16 @busy(i16 1, i8 2)
  %b = call i16 @busy_size(i16 3, i8 4)
  %c = call i16 @quiet(i16 5)
  %s = add i16 %a, %b
  %t = add i16 %s, %c
  ret i16 %t
}
