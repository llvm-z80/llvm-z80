; RUN: llc -verify-machineinstrs -mtriple=z80 -z80-asm-format=z80asm < %s | FileCheck %s

; 1. Function in code_compiler section with GLOBAL directive and branch label
; CHECK:        SECTION code_compiler
; CHECK-NEXT:   GLOBAL	_test_branch
; CHECK-LABEL: _test_branch:
; CHECK:        jr	nz,LBB0_2
; CHECK:        call	_foo
; CHECK:        LBB0_2:
; CHECK:        ret
; CHECK-NOT:   .Lfunc_end
; CHECK-NOT:   .size
; CHECK-NOT:   .type
declare void @foo()
define void @test_branch(i1 %c) {
entry:
  br i1 %c, label %t, label %f
t:
  call void @foo()
  ret void
f:
  ret void
}

; 2. Data sections and directives
; CHECK:        SECTION data_compiler
; CHECK-NEXT:   GLOBAL	_var_16
; CHECK-LABEL: _var_16:
; CHECK-NEXT:   DEFW	1234
@var_16 = dso_local global i16 1234, align 2

; CHECK:        GLOBAL	_var_32
; CHECK-LABEL: _var_32:
; CHECK-NEXT:   DEFQ	305419896
@var_32 = dso_local global i32 305419896, align 4

; CHECK:        GLOBAL	_var_64
; CHECK-LABEL: _var_64:
; CHECK-NEXT:   DEFQ	2309737967
; CHECK-NEXT:   DEFQ	19088743
@var_64 = dso_local global i64 81985529216486895, align 8

; 3. BSS section with DEFS
; CHECK:        SECTION bss_compiler
; CHECK-NEXT:   GLOBAL	_var_bss
; CHECK-LABEL: _var_bss:
; CHECK-NEXT:   DEFS	10
@var_bss = dso_local global [10 x i8] zeroinitializer, align 1

; 4. RoData section with DEFM and dotless symbol name
; CHECK:        SECTION rodata_compiler
; CHECK-LABEL: L__str:
; CHECK-NEXT:   DEFM	"test\000"
@.str = private unnamed_addr constant [5 x i8] c"test\00", align 1

; 5. Verify no ELF directives or sections
; CHECK-NOT:   .text
; CHECK-NOT:   .data
; CHECK-NOT:   .bss
; CHECK-NOT:   .rodata
; CHECK-NOT:   .globl
; CHECK-NOT:   .file
; CHECK-NOT:   .ident
; CHECK-NOT:   .note.GNU-stack
; CHECK-NOT:   __do_copy_data
; CHECK-NOT:   __do_zero_bss
