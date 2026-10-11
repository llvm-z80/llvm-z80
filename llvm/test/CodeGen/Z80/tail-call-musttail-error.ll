; RUN: not llc -mtriple=z80 -O1 < %s -o /dev/null 2>&1 | FileCheck %s
; RUN: not llc -mtriple=sm83 -O1 < %s -o /dev/null 2>&1 | FileCheck %s

; The third argument goes on the stack, which a jump cannot pass.
; CHECK: error: {{.*}}in function three{{.*}}musttail call cannot be lowered as a tail call

declare i16 @g3(i16, i16, i16)

define i16 @three(i16 %a, i16 %b, i16 %c) {
  %r = musttail call i16 @g3(i16 %a, i16 %b, i16 %c)
  ret i16 %r
}
