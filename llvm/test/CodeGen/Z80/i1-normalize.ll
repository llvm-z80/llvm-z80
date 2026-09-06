; RUN: llc -mtriple=z80 -O1 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=sm83 -O1 -verify-machineinstrs < %s | FileCheck %s

; A bool in a register is 0 or 1. G_ZEXT of an s1 selects to a plain COPY on
; that basis, so every producer of an s1 has to hold to it. Truncation is the
; one that can leave the other bits set.

define zeroext i1 @ret_trunc(ptr %p) {
; CHECK-LABEL: ret_trunc:
; CHECK:       and 1
; CHECK-NEXT:  ret
  %v = load volatile i8, ptr %p
  %c = trunc i8 %v to i1
  ret i1 %c
}

define zeroext i1 @ret_trunc16(ptr %p) {
; CHECK-LABEL: ret_trunc16:
; CHECK:       and 1
; CHECK-NEXT:  ret
  %v = load volatile i16, ptr %p
  %c = trunc i16 %v to i1
  ret i1 %c
}

declare void @sink(i1 zeroext)

define void @pass_trunc(ptr %p) {
; CHECK-LABEL: pass_trunc:
; CHECK:       and 1
; CHECK:       call _sink
  %v = load volatile i8, ptr %p
  %c = trunc i8 %v to i1
  call void @sink(i1 zeroext %c)
  ret void
}

; A compare already produces 0 or 1, so extending one stays free.

define i8 @zext_cmp(i8 %x) {
; CHECK-LABEL: zext_cmp:
; CHECK:       sbc a,a
; CHECK-NEXT:  and 1
; CHECK-NEXT:  ret
  %c = icmp ult i8 %x, 7
  %z = zext i1 %c to i8
  ret i8 %z
}

; An i1 is narrower than any register, so it is widened into one and narrowed
; back out. G_ASSERT_ZEXT carries a caller's zeroext promise to the combiner,
; so the narrowing side does not mask a second time.

declare zeroext i1 @source()

define i8 @in_arg(i1 zeroext %b) {
; CHECK-LABEL: in_arg:
; CHECK-NOT:   and
; CHECK:       ret
  %z = zext i1 %b to i8
  ret i8 %z
}

define i8 @in_ret() {
; CHECK-LABEL: in_ret:
; CHECK:       call _source
; CHECK-NEXT:  ret
  %b = call zeroext i1 @source()
  %z = zext i1 %b to i8
  ret i8 %z
}

; Without the promise the value is masked, since nothing else guarantees it.

declare i1 @loose()

define i8 @in_ret_loose() {
; CHECK-LABEL: in_ret_loose:
; CHECK:       call _loose
; CHECK-NEXT:  and 1
; CHECK-NEXT:  ret
  %b = call i1 @loose()
  %z = zext i1 %b to i8
  ret i8 %z
}
