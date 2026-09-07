; RUN: llc -verify-machineinstrs -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s
; RUN: llc -verify-machineinstrs -mtriple=sm83 -z80-asm-format=sdasz80 -O1 < %s \
; RUN:   | FileCheck %s --check-prefix=SM83 --implicit-check-not=ldir \
; RUN:     --implicit-check-not=lddr
;
; SM83 has no block move or block fill, so everything Z80 lowers inline has
; to become a call instead.  The implicit-check-not above is what pins that:
; a block instruction anywhere in the SM83 output fails the test.

declare void @llvm.memcpy.p0.p0.i16(ptr nocapture writeonly, ptr nocapture readonly, i16, i1 immarg)
declare void @llvm.memmove.p0.p0.i16(ptr nocapture, ptr nocapture readonly, i16, i1 immarg)
declare void @llvm.memset.p0.i16(ptr nocapture writeonly, i8, i16, i1 immarg)

@memmove_buf = internal global [64 x i8] zeroinitializer

; LDIR decrements BC before testing it, so a runtime length has to be checked
; for zero first, otherwise a zero copies 65536 bytes.  Four bytes of guard,
; no call.
define void @test_memcpy(ptr %dst, ptr %src, i16 %n) {
; CHECK-LABEL: _test_memcpy:
; SM83-LABEL: _test_memcpy:
; SM83:       call ___z80_memcpy_builtin
; CHECK:       ld a,b
; CHECK-NEXT:  or c
; CHECK-NEXT:  jr z,[[SKIP:\.L[A-Za-z0-9_]+]]
; CHECK:       ldir
; CHECK-NEXT: [[SKIP]]:
; CHECK-NOT:   call
  call void @llvm.memcpy.p0.p0.i16(ptr %dst, ptr %src, i16 %n, i1 false)
  ret void
}

; A proven non-zero constant keeps the compact inline LDIR lowering.
define void @test_memcpy_constant(ptr %dst, ptr %src) {
; CHECK-LABEL: _test_memcpy_constant:
; SM83-LABEL: _test_memcpy_constant:
; SM83:       ld hl,#16
; SM83-NEXT:  call ___z80_memcpy_builtin
; What matters here is that a constant length stays inline: the count goes in
; BC and the copy is a bare LDIR, with no call and no zero guard.  Which
; instructions move the pointers into HL and DE is register allocation, and
; the direction they end up in is pinned in builtin-cc.ll, where global
; addresses make it visible without a shuffle.
; CHECK:       ld bc,#16
; CHECK:       ldir
; CHECK-NOT:   call
  call void @llvm.memcpy.p0.p0.i16(ptr %dst, ptr %src, i16 16, i1 false)
  ret void
}

; Zero-size memcpy must be removed rather than issuing BC=0 LDIR.
define void @test_memcpy_zero(ptr %dst, ptr %src) {
; CHECK-LABEL: _test_memcpy_zero:
; SM83-LABEL: _test_memcpy_zero:
; SM83-NOT:   call
; SM83:       ret
; CHECK-NOT:   ldir
; CHECK-NOT:   call
; CHECK:       ret
  call void @llvm.memcpy.p0.p0.i16(ptr %dst, ptr %src, i16 0, i1 false)
  ret void
}

; Constant-size memmove with dst below src can be proven safe for forward LDIR.
define void @test_memmove_constant_forward() {
; CHECK-LABEL: _test_memmove_constant_forward:
; SM83-LABEL: _test_memmove_constant_forward:
; SM83:       ld hl,#16
; SM83-NEXT:  call ___z80_memmove_builtin
; Copying down onto a lower address is safe forward, so this one stays LDIR
; and addresses the regions from their starts rather than their ends.
; CHECK:       ld bc,#8
; CHECK:       ld hl,#_memmove_buf
; CHECK:       ld bc,#24
; CHECK:       ld hl,#_memmove_buf
; CHECK:       ld bc,#16
; CHECK:       ldir
; CHECK-NOT:   lddr
  %dst = getelementptr i8, ptr @memmove_buf, i16 8
  %src = getelementptr i8, ptr @memmove_buf, i16 24
  call void @llvm.memmove.p0.p0.i16(ptr %dst, ptr %src, i16 16, i1 false)
  ret void
}

; Constant-size memmove with dst above src must use backward LDDR, reading
; from src+size-1 and writing to dst+size-1.  Materialising an end pointer
; goes through HL, so the two of them cannot both be built once one is already
; there, the source end is parked in BC while the destination end is formed.
define void @test_memmove_constant_backward() {
; CHECK-LABEL: _test_memmove_constant_backward:
; SM83-LABEL: _test_memmove_constant_backward:
; SM83:       ld hl,#16
; SM83-NEXT:  call ___z80_memmove_builtin
; A backward copy starts from the last byte of each region, so the two end
; pointers are one short of a full length past their bases: 8+15 and 24+15.
; Getting either offset wrong, or picking LDIR, corrupts the overlap.  The
; moves that put those values into HL and DE are register allocation and are
; not pinned; the overlapping copy is executed for real by
; z80-utils/test-runner/testcases/clang/test_65_block_moves.c.
; CHECK:       ld bc,#23
; CHECK:       ld hl,#_memmove_buf
; CHECK:       ld de,#39
; CHECK:       ld hl,#_memmove_buf
; CHECK:       ld bc,#16
; CHECK:       lddr
; CHECK-NOT:   ldir
  %dst = getelementptr i8, ptr @memmove_buf, i16 24
  %src = getelementptr i8, ptr @memmove_buf, i16 8
  call void @llvm.memmove.p0.p0.i16(ptr %dst, ptr %src, i16 16, i1 false)
  ret void
}

; A forward direction proven from the pointers still needs the zero guard for
; the runtime length, but stays inline.
define void @test_memmove_known_direction_runtime_size(i16 %n) {
; CHECK-LABEL: _test_memmove_known_direction_runtime_size:
; SM83-LABEL: _test_memmove_known_direction_runtime_size:
; SM83:       call ___z80_memmove_builtin
; CHECK:       ld a,b
; CHECK-NEXT:  or c
; CHECK-NEXT:  jr z,[[SKIP:\.L[A-Za-z0-9_]+]]
; CHECK:       ldir
; CHECK-NEXT: [[SKIP]]:
; CHECK-NOT:   lddr
; CHECK-NOT:   call
  %dst = getelementptr i8, ptr @memmove_buf, i16 8
  %src = getelementptr i8, ptr @memmove_buf, i16 24
  call void @llvm.memmove.p0.p0.i16(ptr %dst, ptr %src, i16 %n, i1 false)
  ret void
}

; Zero-size memmove must be removed before direction lowering.
define void @test_memmove_zero(ptr %dst, ptr %src) {
; CHECK-LABEL: _test_memmove_zero:
; SM83-LABEL: _test_memmove_zero:
; SM83-NOT:   call
; SM83:       ret
; CHECK-NOT:   ldir
; CHECK-NOT:   lddr
; CHECK-NOT:   call
; CHECK:       ret
  call void @llvm.memmove.p0.p0.i16(ptr %dst, ptr %src, i16 0, i1 false)
  ret void
}

; With the direction unknown at compile time, the runtime dst-vs-src comparison
; costs more inline than it saves, so a libcall stays, the register-argument
; form, which takes the length in BC rather than on the stack.
define void @test_memmove(ptr %dst, ptr %src, i16 %n) {
; CHECK-LABEL: _test_memmove:
; SM83-LABEL: _test_memmove:
; SM83:       call ___z80_memmove_builtin
; CHECK:       ld c,(hl)
; CHECK:       ld b,(hl)
; CHECK-NOT:   push
; CHECK:       call ___z80_memmove_builtin
  call void @llvm.memmove.p0.p0.i16(ptr %dst, ptr %src, i16 %n, i1 false)
  ret void
}

; Test: memset lowers to the register-argument runtime call, with the i8 value
; promoted to i16 so it lands in DE.
define void @test_memset(ptr %dst, i8 %val, i16 %n) {
; CHECK-LABEL: _test_memset:
; SM83-LABEL: _test_memset:
; SM83:       ld c,a
; SM83-NEXT:  ld b,#0
; SM83-NEXT:  call ___z80_memset_builtin
; CHECK:       call ___z80_memset_builtin
  call void @llvm.memset.p0.i16(ptr %dst, i8 %val, i16 %n, i1 false)
  ret void
}

; Test: memset val is zero-extended (not sign-extended) to i16
define void @test_memset_zext(ptr %dst, i8 %val, i16 %n) {
; CHECK-LABEL: _test_memset_zext:
; SM83-LABEL: _test_memset_zext:
; SM83:       ld c,a
; SM83-NEXT:  ld b,#0
; SM83-NEXT:  call ___z80_memset_builtin
; CHECK:       ld {{[a-z]}},#0
  call void @llvm.memset.p0.i16(ptr %dst, i8 %val, i16 %n, i1 false)
  ret void
}
