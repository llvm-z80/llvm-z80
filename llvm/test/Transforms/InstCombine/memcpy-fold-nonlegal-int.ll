; RUN: opt < %s -passes=instcombine -S | FileCheck %s
; XFAIL: *
;
; DEMONSTRATION TEST (ravn/llvm-z80#87 / #73) — expected to FAIL on current
; upstream; the CHECK lines assert the POST-FIX behavior.  Remove the `XFAIL`
; line once the fix lands.
;
; InstCombine's SimplifyAnyMemTransfer folds a 1/2/4/8-byte memcpy into a single
; load+store.  On a target whose native integer widths do not include the folded
; width (8/16-bit targets: Z80 `n8:16`, AVR), folding an 8-byte memcpy to
; `load i64 / store i64` legalizes into many small loads/stores — far bigger than
; the memcpy/LDIR runtime call it replaced (#87/#73: ~28-40 B of inline stores vs
; ~12 B for LDIR).  The fix gates the fold on `DL.isLegalInteger(Size * 8)`.
;
; Here i64 is NOT a legal integer (datalayout `n8:16`), so the memcpy must be
; preserved.  Pre-fix it is folded to a `store i64` regardless of legality.

target datalayout = "e-m:o-p:16:8-i16:8-i32:8-i64:8-i128:8-f32:8-f64:8-n8:16"

@dst = global [8 x i8] zeroinitializer, align 8
@src = constant [8 x i8] c"\01\02\03\04\05\06\07\08", align 8

define void @copy8() {
; CHECK-LABEL: @copy8(
; CHECK:       call void @llvm.memcpy
; CHECK-NOT:   store i64
  call void @llvm.memcpy.p0.p0.i16(ptr align 8 @dst, ptr align 8 @src, i16 8, i1 false)
  ret void
}

declare void @llvm.memcpy.p0.p0.i16(ptr, ptr, i16, i1)
