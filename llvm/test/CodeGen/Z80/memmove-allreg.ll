; RUN: llc -mtriple=z80 < %s | FileCheck %s
;
; A memmove whose dst-vs-src direction is not statically known lowers to the
; register-CC helper __memmove_rt (CallingConv::Z80_AllReg:
; dst=HL, src=DE, size=BC) instead of the heavy stack-ABI _memmove libcall
; (IX frame + 4(ix) stack-arg read + triplicated callee-cleanup).  With all
; three args already in HL/DE/BC the call site is just `ld bc,N; jp/call
; ___memmove_rt`.  memcpy keeps its inline LDIR lowering.

target datalayout = "e-m:o-p:16:8-i16:8-i32:8-i64:8-i128:8-f32:8-f64:8-n8:16"
target triple = "z80"

declare void @llvm.memmove.p0.p0.i16(ptr, ptr, i16, i1)
declare void @llvm.memcpy.p0.p0.i16(ptr, ptr, i16, i1)

; Two unrelated runtime pointers -> direction unknown -> __memmove_rt.
; CHECK-LABEL: mm:
; CHECK:      ld bc,16
; CHECK:      ___memmove_rt
define void @mm(ptr %d, ptr %s) {
  call void @llvm.memmove.p0.p0.i16(ptr %d, ptr %s, i16 16, i1 false)
  ret void
}

; memcpy still inlines LDIR (no libcall, no frame).
; CHECK-LABEL: mc:
; CHECK:      ldir
; CHECK-NOT:  ___memmove_rt
define void @mc(ptr %d, ptr %s) {
  call void @llvm.memcpy.p0.p0.i16(ptr %d, ptr %s, i16 16, i1 false)
  ret void
}
