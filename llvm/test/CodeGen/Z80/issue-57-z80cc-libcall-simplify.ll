; RUN: opt -passes=instcombine -z80-classic-libc-cc -mtriple=z80-unknown-unknown -S < %s | FileCheck %s --check-prefix=Z80
; RUN: opt -passes=instcombine -z80-classic-libc-cc -mtriple=x86_64-- -S < %s | FileCheck %s --check-prefix=OTHER
;
; Libcall simplification must fire for a classic-ABI C library's printf
; even though it carries an explicit Z80 calling convention rather than
; plain C. Before the fix, TargetLibraryInfoImpl::isCallingConvCCompatible
; returned false for the Z80 clib CCs, so InstCombine's LibCallSimplifier
; refused to rewrite printf("foo\n") -> puts("foo") and the size win was
; lost whenever the classic ABI was honored (target triple supplied on the
; RUN line so both runs share this body).
;
; C source: void print_banner(void) { printf("foo\n"); }

@.str   = private unnamed_addr constant [5 x i8] c"foo\0A\00"
@.str.2 = private unnamed_addr constant [3 x i8] c"%d\00"

declare z80_sdcccall0 i16 @printf(ptr, ...)

; The bug pattern: printf("foo\n"), sdcccall(0) -> puts, cc129 (Z80_SmallC).
define void @print_banner() {
; Z80-LABEL: define void @print_banner()
; Z80:         call cc129 i16 @puts(
; Z80-NOT:     ) @printf(
;
; OTHER-LABEL: define void @print_banner()
; OTHER:         call z80_sdcccall0 i16 (ptr, ...) @printf(
; OTHER-NOT:     @puts
  %call = call z80_sdcccall0 i16 (ptr, ...) @printf(ptr @.str)
  ret void
}

; Safety boundary: a printf that consumes a vararg is not reducible to
; puts/putchar, so it must stay a printf call and keep its CC.
define void @printf_with_arg(i16 %x) {
; Z80-LABEL: define void @printf_with_arg(i16 %x)
; Z80:         call z80_sdcccall0 i16 (ptr, ...) @printf(ptr {{.*}}@.str.2
; Z80-NOT:     call {{.*}}@puts(
; Z80-NOT:     call {{.*}}@putchar(
  %call = call z80_sdcccall0 i16 (ptr, ...) @printf(ptr @.str.2, i16 %x)
  ret void
}
