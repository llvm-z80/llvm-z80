; RUN: opt -passes=instcombine -z80-classic-libc-cc -S < %s | FileCheck %s --check-prefix=SMALLC
; RUN: opt -passes=instcombine -S < %s | FileCheck %s --check-prefix=DEFAULT
;
; InstCombine's LibCallSimplifier rewrites printf("foo\n") -> puts("foo") and
; synthesizes the `puts` declaration itself. A classic-ABI C library's real
; puts routine may use a non-default calling convention (e.g. z88dk's
; __smallc, CallingConv::Z80_SmallC = cc129: stack args, caller cleanup), so
; a call emitted with the default C CC reads stack garbage at runtime. With
; -z80-classic-libc-cc the synthesized decl (and therefore the call, which
; copies the callee CC) is stamped cc129; without the flag the behaviour is
; unchanged (default C CC), so a default-ABI C library is unaffected.
;
; C source: void print_banner(void) { printf("foo\n"); }

target triple = "z80-unknown-unknown"

@.str = private unnamed_addr constant [5 x i8] c"foo\0A\00"

declare i16 @printf(ptr, ...)

define void @print_banner() {
; SMALLC-LABEL: define void @print_banner()
; SMALLC:         call cc129 i16 @puts(
;
; DEFAULT-LABEL: define void @print_banner()
; DEFAULT-NOT:    cc129
; DEFAULT:        call i16 @puts(
  %call = call i16 (ptr, ...) @printf(ptr @.str)
  ret void
}

; The synthesized declaration itself carries the convention under the flag.
; SMALLC: declare cc129 {{.*}}i16 @puts(
; DEFAULT-NOT: declare cc129 {{.*}}@puts(
