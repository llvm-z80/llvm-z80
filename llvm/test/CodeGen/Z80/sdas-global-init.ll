; RUN: llc -verify-machineinstrs -mtriple=z80 -z80-asm-format=sdasz80 < %s | FileCheck %s

; The sdas .ds directive takes no fill operand and only moves the location
; counter along, so neither a repeated non-zero byte nor a run of zeros can use
; it here: this section's bytes are written out, and .ds would leave a hole
; where they belong. See sdcc-zero-fill.ll for the section that can use it.

; CHECK-LABEL: _mixed:
; CHECK-NOT:     .ds
; CHECK-COUNT-4: .db	255
; CHECK-COUNT-4: .db	0
; CHECK-NOT:     .ds
@mixed = dso_local constant [2 x [4 x i8]] [[4 x i8] c"\FF\FF\FF\FF", [4 x i8] zeroinitializer], align 1

; CHECK-LABEL: _rep:
; CHECK-NOT:     .ds
; CHECK-COUNT-4: .db	255
@rep = dso_local constant [4 x i8] c"\FF\FF\FF\FF", align 1

; CHECK-LABEL: _zeros:
; CHECK-NOT:     .ds
; CHECK-COUNT-8: .db	0
; CHECK-NOT:     .ds
@zeros = dso_local constant [8 x i8] zeroinitializer, align 1
