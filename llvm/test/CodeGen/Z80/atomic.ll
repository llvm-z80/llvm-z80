; RUN: llc -verify-machineinstrs -mtriple=z80 -O1 < %s | FileCheck %s
; RUN: llc -verify-machineinstrs -mtriple=sm83 -O1 < %s | FileCheck %s --check-prefix=SM83

; An 8-bit load or store is a single instruction and interrupts are taken
; between instructions, so it is already atomic and needs no libcall. Anything
; wider becomes an __atomic_* libcall the runtime does not provide, so it fails
; to link rather than emit a sequence that only looks atomic.

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

define i8 @atomic_load_i8_seq_cst(ptr %p) nounwind {
; CHECK-LABEL: atomic_load_i8_seq_cst:
; CHECK-NOT:   call
; CHECK:       ld a,(hl)
; SM83-LABEL: atomic_load_i8_seq_cst:
; SM83-NOT:   call
  %v = load atomic i8, ptr %p seq_cst, align 1
  ret i8 %v
}

; A fence orders nothing on a single core with no store buffer.
define void @fence_seq_cst() nounwind {
; CHECK-LABEL: fence_seq_cst:
; CHECK-NOT:   call
; CHECK:       ret
; SM83-LABEL: fence_seq_cst:
; SM83-NOT:   call
; SM83:       ret
  fence seq_cst
  ret void
}
