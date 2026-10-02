; RUN: llc -verify-machineinstrs -mtriple=z80 -z80-asm-format=z88dk < %s \
; RUN:   | FileCheck %s --implicit-check-not='{{^[[:blank:]]*\.}}' \
; RUN:     --implicit-check-not=EXTERN --implicit-check-not=__do_
; PIC is ignored, so no $local alias appears.
; RUN: llc -mtriple=z80 -z80-asm-format=z88dk -relocation-model=pic < %s \
; RUN:   | FileCheck %s --check-prefix=PIC --implicit-check-not='$local'
; RUN: llc -mtriple=z80 -z80-asm-format=z88dk < %s -o /dev/null 2>&1 \
; RUN:   | FileCheck %s --check-prefix=WARN

; CHECK:        SECTION code_compiler
; CHECK-NEXT:   GLOBAL _test_branch
; CHECK-LABEL: _test_branch:
; CHECK:        ld de,(_ext_var)
; CHECK:        jr nz,LBB0_2
; CHECK:        call _foo
; CHECK:      LBB0_2:
; PIC-LABEL:   _test_branch:
; PIC-LABEL:   _var_16:
@ext_var = external global i16
declare void @foo()
define dso_local i16 @test_branch(i1 %c) {
entry:
  %v = load i16, ptr @ext_var
  br i1 %c, label %t, label %f
t:
  call void @foo()
  ret i16 %v
f:
  ret i16 %v
}

; CHECK-LABEL: _addr_lo:
; CHECK:        ld a,(_var_bss+3)&0xff
; CHECK-LABEL: _addr_hi:
; CHECK:        ld a,(_var_bss+3)>>8
define i8 @addr_lo() {
  ret i8 ptrtoint (ptr getelementptr (i8, ptr @var_bss, i16 3) to i8)
}

define i8 @addr_hi() {
  %shr = lshr i16 ptrtoint (ptr getelementptr (i8, ptr @var_bss, i16 3) to i16), 8
  %conv = trunc i16 %shr to i8
  ret i8 %conv
}

; Intrinsics get no EXTERN.
; CHECK-LABEL: _with_lifetime:
define void @with_lifetime() {
  %buf = alloca [4 x i8], align 1
  call void @llvm.lifetime.start.p0(ptr %buf)
  call void @foo()
  call void @llvm.lifetime.end.p0(ptr %buf)
  ret void
}

; CHECK:        SECTION data_compiler
; CHECK-NEXT:   GLOBAL _var_16
; CHECK-NEXT: _var_16:
; CHECK-NEXT:   DEFW 1234
; CHECK:        GLOBAL _var_32
; CHECK-NEXT: _var_32:
; CHECK-NEXT:   DEFQ 305419896
; CHECK:        GLOBAL _var_64
; CHECK-NEXT: _var_64:
; CHECK-NEXT:   DEFQ 2309737967
; CHECK-NEXT:   DEFQ 19088743
@var_16 = dso_local global i16 1234
@var_32 = dso_local global i32 305419896, align 4
@var_64 = dso_local global i64 81985529216486895, align 8

; CHECK:        SECTION bss_compiler
; CHECK-NEXT:   GLOBAL _var_bss
; CHECK-NEXT: _var_bss:
; CHECK-NEXT:   DEFS 10
@var_bss = dso_local global [10 x i8] zeroinitializer

; CHECK:        SECTION rodata_compiler
; CHECK-NEXT: L_@str:
; CHECK-NEXT:   DEFM "test\000"
; CHECK:        GLOBAL _long
; CHECK-NEXT: _long:
; CHECK-NEXT:   DEFM "\042Quoted\042 and \134back\134slashed text, long enough to "
; CHECK-NEXT:   DEFM "wrap!\000"
@.str = private unnamed_addr constant [5 x i8] c"test\00"
@long = constant [54 x i8] c"\22Quoted\22 and \5Cback\5Cslashed text, long enough to wrap!\00"

; A static local keeps a name apart from a global spelled with '_'.
; CHECK:        SECTION data_compiler
; CHECK-NEXT:   GLOBAL _test_counter
; CHECK-NEXT: _test_counter:
; CHECK:      _test@counter:
; CHECK-NEXT:   DEFW 7
@test_counter = global i16 1
@test.counter = internal global i16 7

; CHECK:        SECTION rodata_compiler
; CHECK-NEXT:   GLOBAL _zero_ro
; CHECK-NEXT: _zero_ro:
; CHECK-NEXT:   DEFS 4
@zero_ro = constant [4 x i8] zeroinitializer

; CHECK:        SECTION bss_compiler
; CHECK-NEXT: _zero_local:
; CHECK-NEXT:   DEFS 3
@zero_local = internal global [3 x i8] zeroinitializer

; CHECK:        SECTION code_user
; CHECK-NEXT:   GLOBAL _in_user
; CHECK-NEXT: _in_user:
@in_user = global i8 5, section "code_user"

; A local alias, as static frames use, stays local.
; CHECK-NOT:    GLOBAL
; CHECK:      _local_alias = _var_16
@local_alias = internal alias i16, ptr @var_16

; CHECK-NOT:    global_ctors
; CHECK:        EXTERN _ext_var
; CHECK-NEXT:   EXTERN _foo
@llvm.global_ctors = appending global [0 x { i32, ptr, ptr }] zeroinitializer

; WARN: warning: alignment of 'var_32' is ignored in the z88dk format
; WARN: warning: alignment of 'var_64' is ignored in the z88dk format
; WARN-NOT: warning:
