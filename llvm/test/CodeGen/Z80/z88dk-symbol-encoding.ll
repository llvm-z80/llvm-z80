; RUN: llc -verify-machineinstrs -mtriple=z80 -z80-asm-format=z88dk < %s | FileCheck %s
; RUN: llc -verify-machineinstrs -mtriple=z80 < %s | FileCheck %s --check-prefix=ELF
;
; Regression: test.counter must not collide with C identifier L4_test7_counter.
; External dotted symbols must emit EXTERN and not be treated as temporary.

@test_counter = global i16 1
@L4_test7_counter = global i16 2
@test.counter = internal global i16 3
@.str.1 = private constant [2 x i8] c"x\00"
@a..b = internal global i16 4
@end. = internal global i16 5
@plain = private global i16 6
@g.alias = internal alias i16, ptr @test.counter
declare void @ext.fn()

; CHECK-LABEL: _ref:
; CHECK: call _4__ext2_fn
; CHECK: ld de,_5__test7_counter
; ELF-LABEL: _ref:
; ELF: call _ext.fn
; ELF: ld de,_test.counter
define ptr @ref() {
  call void @ext.fn()
  ret ptr @test.counter
}

; CHECK: GLOBAL _test_counter
; CHECK-NEXT: _test_counter:
; CHECK: GLOBAL _L4_test7_counter
; CHECK-NEXT: _L4_test7_counter:
; CHECK: _5__test7_counter:
; CHECK: _2_L_3_str1_1:
; CHECK: _2__a0_1_b:
; CHECK: _4__end0_:
; CHECK: L_plain:
; CHECK: _2__g5_alias = _5__test7_counter
; CHECK: EXTERN _4__ext2_fn
