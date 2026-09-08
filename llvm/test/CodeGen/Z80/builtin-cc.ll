; RUN: llc -mtriple=z80 -O1 -verify-machineinstrs < %s | FileCheck %s --check-prefix=Z80
; RUN: llc -mtriple=sm83 -O1 -verify-machineinstrs < %s | FileCheck %s --check-prefix=SM83

; CallingConv::Z80_Builtin (cc134) is what the block-move and block-fill
; libcalls are called with: every argument in a register, none on the stack.
; The register order follows __sdcccall(1)'s for the subtarget and then
; continues into the pair that convention does not reach, so it differs
; between the two targets and has to match the runtime assembly in
; compiler-rt/lib/builtins/{z80,sm83}.  Globals are used for the pointers so
; that the register each one lands in is written out rather than inherited
; from the argument registers.

target datalayout = "e-p:16:8-i16:8-i32:8-i64:8-a:8-n8:16"

@dst = global [64 x i8] zeroinitializer
@src = global [64 x i8] zeroinitializer

declare void @llvm.memcpy.p0.p0.i16(ptr, ptr, i16, i1)
declare void @llvm.memmove.p0.p0.i16(ptr, ptr, i16, i1)
declare void @llvm.memset.p0.i16(ptr, i8, i16, i1)

; Z80 has LDIR, so a copy of unknown length is inlined rather than called.
; LDIR reads through HL and writes through DE, and it decrements BC before
; testing it, so a runtime length has to be guarded against zero.
define void @g_memcpy(i16 %n) {
; Z80-LABEL: g_memcpy:
; Z80:         ld c,l
; Z80-NEXT:    ld b,h
; Z80-NEXT:    ld hl,_src
; Z80-NEXT:    ld de,_dst
; Z80-NEXT:    ld a,b
; Z80-NEXT:    or c
; Z80-NEXT:    jr z,
; Z80:         ldir
; Z80-NOT:     call
;
; SM83 has no block move, so the same copy becomes a builtin-convention call
; with dst in DE, src in BC and the length in HL.
; SM83-LABEL: g_memcpy:
; SM83:        ld l,e
; SM83-NEXT:   ld h,d
; SM83-NEXT:   ld de,_dst
; SM83-NEXT:   ld bc,_src
; SM83-NEXT:   call ___z80_memcpy_builtin
  call void @llvm.memcpy.p0.p0.i16(ptr @dst, ptr @src, i16 %n, i1 false)
  ret void
}

; A memmove whose direction is not known statically stays a call on both
; targets.  On Z80 the arguments are HL, DE, BC; on SM83 they are DE, BC, HL.
define void @g_memmove(i16 %n) {
; Z80-LABEL: g_memmove:
; Z80:         ld c,l
; Z80-NEXT:    ld b,h
; Z80-NEXT:    ld hl,_dst
; Z80-NEXT:    ld de,_src
; Z80-NEXT:    call ___z80_memmove_builtin
;
; SM83-LABEL: g_memmove:
; SM83:        ld l,e
; SM83-NEXT:   ld h,d
; SM83-NEXT:   ld de,_dst
; SM83-NEXT:   ld bc,_src
; SM83-NEXT:   call ___z80_memmove_builtin
  call void @llvm.memmove.p0.p0.i16(ptr @dst, ptr @src, i16 %n, i1 false)
  ret void
}

; memset's byte is widened to 16 bits so that it takes a whole pair rather
; than the accumulator, which is where the runtime expects to find it.
define void @g_memset(i16 %n) {
; Z80-LABEL: g_memset:
; Z80:         ld c,l
; Z80-NEXT:    ld b,h
; Z80-NEXT:    ld hl,_dst
; Z80-NEXT:    ld de,7
; Z80-NEXT:    call ___z80_memset_builtin
;
; SM83-LABEL: g_memset:
; SM83:        ld l,e
; SM83-NEXT:   ld h,d
; SM83-NEXT:   ld de,_dst
; SM83-NEXT:   ld bc,7
; SM83-NEXT:   call ___z80_memset_builtin
  call void @llvm.memset.p0.i16(ptr @dst, i8 7, i16 %n, i1 false)
  ret void
}

; A copy of statically known zero length performs no accesses, so nothing at
; all should be emitted for it.
define void @g_memcpy_zero() {
; Z80-LABEL: g_memcpy_zero:
; Z80-NOT:     ldir
; Z80-NOT:     call
; Z80:         ret
;
; SM83-LABEL: g_memcpy_zero:
; SM83-NOT:    call
; SM83:        ret
  call void @llvm.memcpy.p0.p0.i16(ptr @dst, ptr @src, i16 0, i1 false)
  ret void
}
