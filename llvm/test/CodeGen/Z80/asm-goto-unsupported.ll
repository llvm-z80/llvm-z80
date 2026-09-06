; RUN: not llc -mtriple=z80 -O1 < %s 2>&1 | FileCheck %s
; RUN: not llc -mtriple=sm83 -O0 < %s 2>&1 | FileCheck %s

; asm goto (an inline-asm callbr) has no GlobalISel lowering; it must fail
; with a proper diagnostic, not an internal error.

; CHECK: error:
; CHECK-SAME: asm goto is not supported
; CHECK-NOT: unable to translate

define i16 @foo(i16 %x) {
entry:
  callbr void asm sideeffect "", "r,!i"(i16 %x)
          to label %fallthrough [label %out]

fallthrough:
  ret i16 0

out:
  ret i16 1
}

; An indirect destination can repeat the default (or another indirect)
; destination; each entry is a separate edge with its own PHI entry, and
; the replacement branch must leave exactly one.
define i16 @dup_dest(i16 %x) {
entry:
  callbr void asm sideeffect "", "r,!i,!i"(i16 %x)
          to label %join [label %join, label %join]

join:
  %r = phi i16 [ 7, %entry ], [ 7, %entry ], [ 7, %entry ]
  ret i16 %r
}
