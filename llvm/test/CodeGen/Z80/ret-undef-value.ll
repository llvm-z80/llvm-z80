; RUN: llc -verify-machineinstrs -mtriple=z80 -O1 < %s | FileCheck %s
; RUN: llc -verify-machineinstrs -mtriple=sm83 -O1 < %s -o /dev/null

; A function that falls off its end returns a value nothing produced. The
; callee-cleanup return is a pseudo, and the sequence it expands into has to
; keep saying that the value register holds nothing.

define i16 @falls_off(i16 %a, i16 %b, i16 %c) {
; CHECK-LABEL: falls_off:
entry:
  %cmp = icmp eq i16 %a, %b
  br i1 %cmp, label %call, label %end

call:
  call void @sink(i16 %c)
  br label %end

end:
  ret i16 undef
}

declare void @sink(i16)
