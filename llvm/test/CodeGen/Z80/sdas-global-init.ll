; RUN: llc -verify-machineinstrs -mtriple=z80 -z80-asm-format=sdasz80 < %s | FileCheck %s

; The sdas .ds directive reserves zeroed space and takes no fill operand, so a
; repeated non-zero byte has to be written out as .db.

; CHECK-LABEL: _mixed:
; CHECK-NOT:   .ds{{.*}},
; CHECK:       .db	255
; CHECK:       .db	255
; CHECK:       .db	255
; CHECK:       .db	255
; CHECK:       .ds	4
@mixed = dso_local constant [2 x [4 x i8]] [[4 x i8] c"\FF\FF\FF\FF", [4 x i8] zeroinitializer], align 1

; CHECK-LABEL: _rep:
; CHECK-NOT:   .ds{{.*}},
; CHECK:       .db	255
@rep = dso_local constant [4 x i8] c"\FF\FF\FF\FF", align 1

; An all-zero initializer still uses the compact form.
; CHECK-LABEL: _zeros:
; CHECK:       .ds	8
@zeros = dso_local constant [8 x i8] zeroinitializer, align 1
