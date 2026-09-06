; RUN: llc -verify-machineinstrs -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s

declare void @llvm.memcpy.p0.p0.i16(ptr nocapture writeonly, ptr nocapture readonly, i16, i1 immarg)
declare void @llvm.memmove.p0.p0.i16(ptr nocapture, ptr nocapture readonly, i16, i1 immarg)
declare void @llvm.memset.p0.i16(ptr nocapture writeonly, i8, i16, i1 immarg)

@memmove_buf = internal global [64 x i8] zeroinitializer

; Variable-size memcpy uses the guarded register-CC helper because BC=0
; would make a raw LDIR copy 65536 bytes.
define void @test_memcpy(ptr %dst, ptr %src, i16 %n) {
; CHECK-LABEL: _test_memcpy:
; CHECK:       call ___memmove_rt
  call void @llvm.memcpy.p0.p0.i16(ptr %dst, ptr %src, i16 %n, i1 false)
  ret void
}

; A proven non-zero constant keeps the compact inline LDIR lowering.
define void @test_memcpy_constant(ptr %dst, ptr %src) {
; CHECK-LABEL: _test_memcpy_constant:
; The incoming pointers are HL=dst and DE=src; memcpy swaps them for LDIR:
; HL=src, DE=dst, BC=16.
; CHECK:       ld c,l
; CHECK-NEXT:  ld b,h
; CHECK-NEXT:  ex de,hl
; CHECK-NEXT:  ld e,c
; CHECK-NEXT:  ld d,b
; CHECK-NEXT:  ld bc,#16
; CHECK:       ldir
  call void @llvm.memcpy.p0.p0.i16(ptr %dst, ptr %src, i16 16, i1 false)
  ret void
}

; Zero-size memcpy must be removed rather than issuing BC=0 LDIR.
define void @test_memcpy_zero(ptr %dst, ptr %src) {
; CHECK-LABEL: _test_memcpy_zero:
; CHECK-NOT:   ldir
; CHECK-NOT:   ___memmove_rt
; CHECK:       ret
  call void @llvm.memcpy.p0.p0.i16(ptr %dst, ptr %src, i16 0, i1 false)
  ret void
}

; Constant-size memmove with dst below src can be proven safe for forward LDIR.
define void @test_memmove_constant_forward() {
; CHECK-LABEL: _test_memmove_constant_forward:
; CHECK:       ld bc,#8
; CHECK:       ld hl,#_memmove_buf
; CHECK:       add hl,bc
; CHECK:       ex de,hl
; CHECK:       ld bc,#24
; CHECK:       ld hl,#_memmove_buf
; CHECK:       add hl,bc
; CHECK:       ld bc,#16
; CHECK:       ldir
; CHECK-NOT:   lddr
  %dst = getelementptr i8, ptr @memmove_buf, i16 8
  %src = getelementptr i8, ptr @memmove_buf, i16 24
  call void @llvm.memmove.p0.p0.i16(ptr %dst, ptr %src, i16 16, i1 false)
  ret void
}

; Constant-size memmove with dst above src must use backward LDDR.
define void @test_memmove_constant_backward() {
; CHECK-LABEL: _test_memmove_constant_backward:
; CHECK:       ld bc,#23
; CHECK:       ld hl,#_memmove_buf
; CHECK:       add hl,bc
; CHECK:       ld c,l
; CHECK:       ld b,h
; CHECK:       ld de,#39
; CHECK:       ld hl,#_memmove_buf
; CHECK:       add hl,de
; CHECK:       ex de,hl
; CHECK:       ld l,c
; CHECK:       ld h,b
; CHECK:       ld bc,#16
; CHECK:       lddr
; CHECK-NOT:   ldir
  %dst = getelementptr i8, ptr @memmove_buf, i16 24
  %src = getelementptr i8, ptr @memmove_buf, i16 8
  call void @llvm.memmove.p0.p0.i16(ptr %dst, ptr %src, i16 16, i1 false)
  ret void
}

; Known pointer direction with runtime size still needs the zero-safe helper.
define void @test_memmove_known_direction_runtime_size(i16 %n) {
; CHECK-LABEL: _test_memmove_known_direction_runtime_size:
; CHECK:       call ___memmove_rt
; CHECK-NOT:   ldir
; CHECK-NOT:   lddr
  %dst = getelementptr i8, ptr @memmove_buf, i16 8
  %src = getelementptr i8, ptr @memmove_buf, i16 24
  call void @llvm.memmove.p0.p0.i16(ptr %dst, ptr %src, i16 %n, i1 false)
  ret void
}

; Zero-size memmove must be removed before direction lowering.
define void @test_memmove_zero(ptr %dst, ptr %src) {
; CHECK-LABEL: _test_memmove_zero:
; CHECK-NOT:   ldir
; CHECK-NOT:   lddr
; CHECK-NOT:   ___memmove_rt
; CHECK:       ret
  call void @llvm.memmove.p0.p0.i16(ptr %dst, ptr %src, i16 0, i1 false)
  ret void
}

; Test: unknown-direction memmove uses the all-register helper.
define void @test_memmove(ptr %dst, ptr %src, i16 %n) {
; CHECK-LABEL: _test_memmove:
; CHECK:       call ___memmove_rt
  call void @llvm.memmove.p0.p0.i16(ptr %dst, ptr %src, i16 %n, i1 false)
  ret void
}

; Test: memset lowers to runtime call with i8 val promoted to i16
define void @test_memset(ptr %dst, i8 %val, i16 %n) {
; CHECK-LABEL: _test_memset:
; CHECK:       call _memset
  call void @llvm.memset.p0.i16(ptr %dst, i8 %val, i16 %n, i1 false)
  ret void
}

; Test: memset val is zero-extended (not sign-extended) to i16
define void @test_memset_zext(ptr %dst, i8 %val, i16 %n) {
; CHECK-LABEL: _test_memset_zext:
; CHECK:       ld {{[a-z]}},#0
  call void @llvm.memset.p0.i16(ptr %dst, i8 %val, i16 %n, i1 false)
  ret void
}
