; RUN: llc -verify-machineinstrs -mtriple=z80 -O2 < %s | FileCheck %s

; Test that G_PTR_ADD does not evict the offset from HL when the offset
; was just computed/shifted into HL, avoiding redundant LD BC,HL and LD HL,BC.

@table = external constant [8 x [4 x i8]], align 1
@table_3d = external constant [2 x [4 x [2 x i8]]], align 1
@a = external global i8, align 1
@b = external global i8, align 1

; Shape (a): Exact pattern: GEP where offset is shifted in HL and added to global base
; CHECK-LABEL: gep_global_shifted_offset:
; CHECK:       ld l,a
; CHECK:       add hl,hl
; CHECK:       add hl,hl
; CHECK-NOT:   ld c,l
; CHECK-NOT:   ld b,h
; CHECK:       add hl,{{de|bc}}
define ptr @gep_global_shifted_offset(i8 %idx) {
entry:
  %idx.ext = zext i8 %idx to i16
  %gep = getelementptr inbounds [4 x i8], ptr @table, i16 %idx.ext
  ret ptr %gep
}

; Shape (b): Structural variation: Multi-dimensional GEP chain
; CHECK-LABEL: gep_chained_3d:
; CHECK:       ld bc,_table_3d
; CHECK-NEXT:  add hl,bc
; CHECK:       add hl,hl
; CHECK-NEXT:  add hl,bc
; CHECK-NOT:   ld l,c
; CHECK:       ret
define ptr @gep_chained_3d() {
entry:
  %0 = load i8, ptr @a, align 1
  %idx0 = zext i8 %0 to i16
  %p0 = getelementptr inbounds [8 x i8], ptr @table_3d, i16 %idx0
  %1 = load i8, ptr @b, align 1
  %idx1 = zext i8 %1 to i16
  %p1 = getelementptr inbounds [2 x i8], ptr %p0, i16 %idx1
  ret ptr %p1
}

; Shape (c): Positive control: Base pointer already in HL, offset in DE
; CHECK-LABEL: gep_base_in_hl:
; CHECK:       add hl,de
; CHECK-NEXT:  ex de,hl
; CHECK-NEXT:  ret
define ptr @gep_base_in_hl(ptr %base, i16 %offset) {
entry:
  %gep = getelementptr inbounds i8, ptr %base, i16 %offset
  ret ptr %gep
}

; Shape (d): Safety / boundary case: GEP across basic blocks
; CHECK-LABEL: gep_cross_bb:
; CHECK:       jr nz,
; CHECK:       add hl,bc
; CHECK:       ret
define ptr @gep_cross_bb(i1 %cond, ptr %p1, ptr %p2, i16 %off) {
entry:
  br i1 %cond, label %then, label %else

then:
  %p1.off = getelementptr inbounds i8, ptr %p1, i16 %off
  ret ptr %p1.off

else:
  ret ptr %p2
}
