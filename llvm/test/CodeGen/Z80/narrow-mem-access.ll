; RUN: llc -mtriple=z80 -O2 -stop-after=z80-narrow-mem-access < %s | FileCheck %s
; RUN: llc -mtriple=sm83 -O2 -stop-after=z80-narrow-mem-access < %s | FileCheck %s
; RUN: llc -mtriple=z80 -O2 < %s | FileCheck %s --check-prefix=ASM

; SROA writes a field update on a struct copied as an i64 as a mask and an or
; on the whole integer. Only the byte that changes is stored, and only the
; byte that is read is loaded.
define void @update_field(ptr %p) {
; CHECK-LABEL: define void @update_field(
; CHECK:         [[A:%.*]] = getelementptr inbounds i8, ptr %p, i16 1
; CHECK-NEXT:    [[B:%.*]] = load i8, ptr [[A]], align 1
; CHECK-NEXT:    [[INC:%.*]] = add i8 [[B]], 1
; CHECK-NEXT:    [[C:%.*]] = getelementptr inbounds i8, ptr %p, i16 1
; CHECK-NEXT:    store i8 [[INC]], ptr [[C]], align 1
; CHECK-NEXT:    ret void
;
; ASM-LABEL: update_field:
; ASM:         inc hl
; ASM-NEXT:    ld a,(hl)
; ASM-NEXT:    inc a
; ASM-NEXT:    ld (hl),a
; ASM-NEXT:    ret
  %v = load i64, ptr %p, align 1
  %sh = lshr i64 %v, 8
  %b = trunc i64 %sh to i8
  %inc = add i8 %b, 1
  %ext = zext i8 %inc to i64
  %pos = shl i64 %ext, 8
  %keep = and i64 %v, -65281
  %new = or disjoint i64 %pos, %keep
  store i64 %new, ptr %p, align 1
  ret void
}

; Two fields, one of them sixteen bits wide.
define void @update_two_fields(ptr %p, i8 %a, i16 %b) {
; CHECK-LABEL: define void @update_two_fields(
; CHECK-NOT:     load
; CHECK:         store i8 %a, ptr %p, align 1
; CHECK:         [[LO:%.*]] = trunc i16 %b to i8
; CHECK:         store i8 [[LO]], ptr {{%.*}}, align 1
; CHECK:         [[SH:%.*]] = lshr i16 %b, 8
; CHECK-NEXT:    [[HI:%.*]] = trunc i16 [[SH]] to i8
; CHECK:         store i8 [[HI]], ptr {{%.*}}, align 1
; CHECK-NOT:     store
; CHECK:         ret void
  %v = load i32, ptr %p, align 1
  %ea = zext i8 %a to i32
  %eb = zext i16 %b to i32
  %pb = shl i32 %eb, 16
  %keep = and i32 %v, 65280
  %t = or i32 %keep, %ea
  %new = or i32 %t, %pb
  store i32 %new, ptr %p, align 1
  ret void
}

; Writing back what was just loaded does nothing.
define void @write_back(ptr %p) {
; CHECK-LABEL: define void @write_back(
; CHECK-NEXT:    ret void
  %v = load i64, ptr %p, align 1
  store i64 %v, ptr %p, align 1
  ret void
}

; The new value is needed whole anyway, so rebuilding the changed bytes one
; by one would only add to the work.
define i32 @value_used_elsewhere(ptr %p) {
; CHECK-LABEL: define i32 @value_used_elsewhere(
; CHECK:         store i32 %new, ptr %p, align 1
  %v = load i32, ptr %p, align 1
  %new = xor i32 %v, 305419776
  store i32 %new, ptr %p, align 1
  ret i32 %new
}

; A 16-bit store that changes only its low byte.
define void @update_low_byte(ptr %p, i8 %x) {
; CHECK-LABEL: define void @update_low_byte(
; CHECK-NEXT:    store i8 %x, ptr %p, align 1
; CHECK-NEXT:    ret void
  %v = load i16, ptr %p, align 1
  %keep = and i16 %v, -256
  %ext = zext i8 %x to i16
  %new = or disjoint i16 %keep, %ext
  store i16 %new, ptr %p, align 1
  ret void
}

; The field sits at a constant offset from the pointer both accesses share.
define void @update_at_offset(ptr %base, i8 %x) {
; CHECK-LABEL: define void @update_at_offset(
; CHECK:         [[Q:%.*]] = getelementptr inbounds {{.*}}ptr %base, i16 4
; CHECK-NEXT:    [[R:%.*]] = getelementptr inbounds i8, ptr [[Q]], i16 3
; CHECK-NEXT:    store i8 %x, ptr [[R]], align 1
; CHECK-NEXT:    ret void
  %q = getelementptr inbounds i8, ptr %base, i16 4
  %v = load i32, ptr %q, align 1
  %keep = and i32 %v, 16777215
  %ext = zext i8 %x to i32
  %pos = shl i32 %ext, 24
  %new = or i32 %keep, %pos
  store i32 %new, ptr %q, align 1
  ret void
}

; A store in between may change the bytes the load read, so they are all
; written again.
define void @clobbered(ptr %p, ptr %other, i8 %x) {
; CHECK-LABEL: define void @clobbered(
; CHECK:         store i8 0, ptr %other
; CHECK:         store i32 {{%.*}}, ptr %p, align 1
  %v = load i32, ptr %p, align 1
  store i8 0, ptr %other
  %keep = and i32 %v, -256
  %ext = zext i8 %x to i32
  %new = or i32 %keep, %ext
  store i32 %new, ptr %p, align 1
  ret void
}

; The bytes kept come from somewhere else, so the store is a copy.
define void @other_address(ptr %p, ptr %q, i8 %x) {
; CHECK-LABEL: define void @other_address(
; CHECK:         store i32 {{%.*}}, ptr %p, align 1
  %v = load i32, ptr %q, align 1
  %keep = and i32 %v, -256
  %ext = zext i8 %x to i32
  %new = or i32 %keep, %ext
  store i32 %new, ptr %p, align 1
  ret void
}

define void @volatile_load(ptr %p, i8 %x) {
; CHECK-LABEL: define void @volatile_load(
; CHECK:         load volatile i32, ptr %p
; CHECK:         store i32 {{%.*}}, ptr %p, align 1
  %v = load volatile i32, ptr %p, align 1
  %keep = and i32 %v, -256
  %ext = zext i8 %x to i32
  %new = or i32 %keep, %ext
  store i32 %new, ptr %p, align 1
  ret void
}

define void @volatile_store(ptr %p, i8 %x) {
; CHECK-LABEL: define void @volatile_store(
; CHECK:         store volatile i32 {{%.*}}, ptr %p, align 1
  %v = load i32, ptr %p, align 1
  %keep = and i32 %v, -256
  %ext = zext i8 %x to i32
  %new = or i32 %keep, %ext
  store volatile i32 %new, ptr %p, align 1
  ret void
}

; Only one byte of the load is used.
define i8 @read_byte(ptr %p) {
; CHECK-LABEL: define i8 @read_byte(
; CHECK-NEXT:    [[A:%.*]] = getelementptr inbounds i8, ptr %p, i16 3
; CHECK-NEXT:    [[B:%.*]] = load i8, ptr [[A]], align 1
; CHECK-NEXT:    ret i8 [[B]]
  %v = load i64, ptr %p, align 1
  %sh = lshr i64 %v, 24
  %b = trunc i64 %sh to i8
  ret i8 %b
}

; The top byte, zero-extended back to the full width.
define i32 @read_top_byte(ptr %p) {
; CHECK-LABEL: define i32 @read_top_byte(
; CHECK-NEXT:    [[A:%.*]] = getelementptr inbounds i8, ptr %p, i16 3
; CHECK-NEXT:    [[B:%.*]] = load i8, ptr [[A]], align 1
; CHECK-NEXT:    [[C:%.*]] = zext i8 [[B]] to i32
; CHECK-NEXT:    ret i32 [[C]]
  %v = load i32, ptr %p, align 1
  %sh = lshr i32 %v, 24
  ret i32 %sh
}

; Most of the load is used, so it stays whole.
define i16 @read_most(ptr %p) {
; CHECK-LABEL: define i16 @read_most(
; CHECK-NEXT:    load i32, ptr %p, align 1
  %v = load i32, ptr %p, align 1
  %lo = trunc i32 %v to i16
  %sh = lshr i32 %v, 16
  %hi = trunc i32 %sh to i8
  %e = zext i8 %hi to i16
  %r = add i16 %lo, %e
  ret i16 %r
}

define i8 @read_byte_volatile(ptr %p) {
; CHECK-LABEL: define i8 @read_byte_volatile(
; CHECK-NEXT:    load volatile i64, ptr %p, align 1
  %v = load volatile i64, ptr %p, align 1
  %sh = lshr i64 %v, 24
  %b = trunc i64 %sh to i8
  ret i8 %b
}

; Z80 reads both bytes of a 16-bit global into a pair in one instruction, but
; one byte only into A, so a 16-bit load stays whole.
define i8 @read_byte_of_pair(ptr %p) {
; CHECK-LABEL: define i8 @read_byte_of_pair(
; CHECK-NEXT:    load i16, ptr %p, align 1
  %v = load i16, ptr %p, align 1
  %sh = lshr i16 %v, 8
  %b = trunc i16 %sh to i8
  ret i8 %b
}
