; RUN: llc -verify-machineinstrs -mtriple=sm83 -O1 < %s | FileCheck %s

; A compile-time address needs no pointer in a register. The high page
; 0xFF00-0xFFFF has a 2-byte LDH form; anywhere else takes the 3-byte absolute
; form, against 4 bytes for loading a pair and going indirect.

define void @store_high_page(i8 %v) {
; CHECK-LABEL: store_high_page:
; CHECK:       ldh
  store volatile i8 %v, ptr inttoptr (i16 65535 to ptr)
  ret void
}

define i8 @load_high_page() {
; CHECK-LABEL: load_high_page:
; CHECK:       ldh
  %v = load volatile i8, ptr inttoptr (i16 65280 to ptr)
  ret i8 %v
}

; One byte below the high page: LDH cannot reach it.
define void @store_absolute(i8 %v) {
; CHECK-LABEL: store_absolute:
; CHECK-NOT:   ldh
  store volatile i8 %v, ptr inttoptr (i16 65279 to ptr)
  ret void
}

define i8 @load_absolute() {
; CHECK-LABEL: load_absolute:
; CHECK-NOT:   ldh
  %v = load volatile i8, ptr inttoptr (i16 38912 to ptr)
  ret i8 %v
}

; Read-modify-write folds both accesses and leaves no address in a pair.
define void @set_bit() {
; CHECK-LABEL: set_bit:
; CHECK:       ldh
; CHECK:       ldh
  %v = load volatile i8, ptr inttoptr (i16 65535 to ptr)
  %n = or i8 %v, 4
  store volatile i8 %n, ptr inttoptr (i16 65535 to ptr)
  ret void
}
