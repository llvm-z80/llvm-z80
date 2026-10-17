; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 -z80-verify-inline-runtime-size < %s | FileCheck %s

; Variable-size block operations must guard BC==0 because LDIR/LDDR
; interpret zero as 65536 iterations.
; Lowering a zero count to a raw block instruction would run 65536
; iterations (BC test happens AFTER decrement), trashing 64 KB of memory.
;
; Constant-size sub-cases are handled separately (size==0 -> erase).
; This test covers the variable-size case: the legalizer emits guarded
; block-operation pseudos which expand at post-RA into a runtime guard
; that skips the block operation when BC == 0.
;
; Expected expansion of the guarded LDIR:
;   LD A, B
;   OR C            ; sets Z if BC == 0
;   JR Z, .skip
;   LDIR
; .skip:

declare void @llvm.memcpy.p0.p0.i16(ptr nocapture writeonly,
                                    ptr nocapture readonly,
                                    i16, i1 immarg)
declare void @llvm.memmove.p0.p0.i16(ptr nocapture,
                                     ptr nocapture readonly,
                                     i16, i1 immarg)

@memmove_buf = internal global [64 x i8] zeroinitializer

; CHECK-LABEL: _test_memcpy_var:
; The size-zero guard must precede LDIR.
; CHECK:       ld      a,b
; CHECK-NEXT:  or      c
; CHECK-NEXT:  jr      z,
; CHECK:       ldir

define void @test_memcpy_var(ptr %dst, ptr %src, i16 %n) {
entry:
  call void @llvm.memcpy.p0.p0.i16(ptr %dst, ptr %src, i16 %n, i1 false)
  ret void
}

; CHECK-LABEL: _test_memmove_backward_var:
; The size-zero guard must precede LDDR too.
; CHECK:       ld      a,b
; CHECK-NEXT:  or      c
; CHECK-NEXT:  jr      z,
; CHECK:       lddr

define void @test_memmove_backward_var(i16 %n) {
entry:
  %dst = getelementptr i8, ptr @memmove_buf, i16 24
  %src = getelementptr i8, ptr @memmove_buf, i16 8
  call void @llvm.memmove.p0.p0.i16(ptr %dst, ptr %src, i16 %n, i1 false)
  ret void
}
