; RUN: llc -mtriple=z80 -O1 < %s | FileCheck %s
; RUN: llc -mtriple=sm83 -O1 < %s | FileCheck %s --check-prefix=SM83

; Z80 is single-threaded: all atomic operations should be lowered
; to non-atomic load/store sequences (no libcall).

define i8 @atomic_load_i8(ptr %p) nounwind {
; CHECK-LABEL: atomic_load_i8:
; CHECK-NOT:   call
; CHECK:       ld a,(hl)
; CHECK:       ret
; SM83-LABEL: atomic_load_i8:
; SM83-NOT:   call
; SM83:       ret
  %v = load atomic i8, ptr %p monotonic, align 1
  ret i8 %v
}

define void @atomic_store_i8(ptr %p, i8 %val) nounwind {
; CHECK-LABEL: atomic_store_i8:
; CHECK-NOT:   call
; CHECK:       ld (hl),a
; SM83-LABEL: atomic_store_i8:
; SM83-NOT:   call
  store atomic i8 %val, ptr %p monotonic, align 1
  ret void
}

define i8 @atomic_rmw_add_i8(ptr %p, i8 %val) nounwind {
; CHECK-LABEL: atomic_rmw_add_i8:
; CHECK-NOT:   call
; CHECK:       ld a,(hl)
; CHECK:       add a,
; CHECK:       ld (hl),a
; SM83-LABEL: atomic_rmw_add_i8:
; SM83-NOT:   call
  %old = atomicrmw add ptr %p, i8 %val monotonic
  ret i8 %old
}

define i8 @atomic_rmw_xchg_i8(ptr %p, i8 %val) nounwind {
; CHECK-LABEL: atomic_rmw_xchg_i8:
; CHECK-NOT:   call
; CHECK:       ld a,(hl)
; CHECK:       ld (hl),
; SM83-LABEL: atomic_rmw_xchg_i8:
; SM83-NOT:   call
  %old = atomicrmw xchg ptr %p, i8 %val monotonic
  ret i8 %old
}

define i16 @atomic_load_i16(ptr %p) nounwind {
; CHECK-LABEL: atomic_load_i16:
; CHECK-NOT:   call
; SM83-LABEL: atomic_load_i16:
; SM83-NOT:   call
  %v = load atomic i16, ptr %p monotonic, align 2
  ret i16 %v
}

define i16 @atomic_rmw_add_i16(ptr %p, i16 %val) nounwind {
; CHECK-LABEL: atomic_rmw_add_i16:
; CHECK-NOT:   call
; SM83-LABEL: atomic_rmw_add_i16:
; SM83-NOT:   call
  %old = atomicrmw add ptr %p, i16 %val monotonic
  ret i16 %old
}
