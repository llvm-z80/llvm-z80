; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s
; RUN: llc -mtriple=sm83 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s

; Test G_BLOCK_ADDR: computed goto (GCC &&label extension)
; Block addresses should be materialized as LD rr,#label

define i8 @computed_goto(i8 %op) {
; CHECK-LABEL: _computed_goto:
; CHECK:       ld {{[a-z]+}},#{{\.?}}Ltmp
; CHECK:       ld {{[a-z]+}},#{{\.?}}Ltmp
; CHECK:       jp (hl)
entry:
  %table = alloca [2 x ptr]
  store ptr blockaddress(@computed_goto, %bb_a), ptr %table
  %p1 = getelementptr ptr, ptr %table, i32 1
  store ptr blockaddress(@computed_goto, %bb_b), ptr %p1
  %idx = zext i8 %op to i16
  %gep = getelementptr ptr, ptr %table, i16 %idx
  %target = load ptr, ptr %gep
  indirectbr ptr %target, [label %bb_a, label %bb_b]

bb_a:
  ret i8 10

bb_b:
  ret i8 20
}
