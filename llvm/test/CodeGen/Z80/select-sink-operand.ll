; RUN: opt -mtriple=z80 -passes='require<profile-summary>,function(codegenprepare)' -S < %s | FileCheck %s
; RUN: opt -mtriple=sm83 -passes='require<profile-summary>,function(codegenprepare)' -S < %s | FileCheck %s
;
; CodeGenPrepare turns a select into a branch. An operand used only by the
; select either moves into the arm or stays before the branch.
;   1. A compare: stays.
;   2. A cast of a compare result: stays.
;   3. No code of its own: stays.
;   4. Other work that is not integer arithmetic: moves.
;   5. Integer arithmetic: moves.
;   6. Integer arithmetic leaving a byte shared by both arms: stays.
;      A byte is shared when both arms take the byte from the same value.

@g = global i16 0, align 1

declare i16 @llvm.umin.i16(i16, i16)

define i1 @keep_compare(i16 %x, i16 %z, i1 %c) {
; CHECK-LABEL: @keep_compare(
; CHECK-NEXT:    %a = icmp ult i16 %x, %z
; CHECK-NOT:   sink
; CHECK:       ret i1
  %a = icmp ult i16 %x, %z
  %r = select i1 %c, i1 %a, i1 false
  ret i1 %r
}

define i8 @keep_bool_cast(i1 %b, i8 %y, i1 %c) {
; CHECK-LABEL: @keep_bool_cast(
; CHECK-NEXT:    %a = zext i1 %b to i8
; CHECK-NOT:   sink
; CHECK:       ret i8
  %a = zext i1 %b to i8
  %r = select i1 %c, i8 %a, i8 %y
  ret i8 %r
}

define i16 @keep_freeze(i16 %x, i16 %y, i1 %c) {
; CHECK-LABEL: @keep_freeze(
; CHECK-NEXT:    %a = freeze i16 %x
; CHECK-NOT:   sink
; CHECK:       ret i16
  %a = freeze i16 %x
  %r = select i1 %c, i16 %a, i16 %y
  ret i16 %r
}

define i16 @keep_ptrtoint(ptr %p, i16 %y, i1 %c) {
; CHECK-LABEL: @keep_ptrtoint(
; CHECK-NEXT:    %a = ptrtoint ptr %p to i16
; CHECK-NOT:   sink
; CHECK:       ret i16
  %a = ptrtoint ptr %p to i16
  %r = select i1 %c, i16 %a, i16 %y
  ret i16 %r
}

define i16 @move_load(i16 %y, i1 %c) {
; CHECK-LABEL: @move_load(
; CHECK:       select.true.sink:
; CHECK-NEXT:    %a = load i16, ptr @g
  %a = load i16, ptr @g, align 1
  %r = select i1 %c, i16 %a, i16 %y
  ret i16 %r
}

define float @move_float(float %x, float %z, float %y, i1 %c) {
; CHECK-LABEL: @move_float(
; CHECK:       select.true.sink:
; CHECK-NEXT:    %a = fmul float %x, %z
  %a = fmul float %x, %z
  %r = select i1 %c, float %a, float %y
  ret float %r
}

define i16 @move_call(i16 %x, i16 %z, i16 %y, i1 %c) {
; CHECK-LABEL: @move_call(
; CHECK:       select.true.sink:
; CHECK-NEXT:    %a = call i16 @llvm.umin.i16(i16 %x, i16 %z)
  %a = call i16 @llvm.umin.i16(i16 %x, i16 %z)
  %r = select i1 %c, i16 %a, i16 %y
  ret i16 %r
}

define ptr @move_address(ptr %p, ptr %q, i1 %c) {
; CHECK-LABEL: @move_address(
; CHECK:       select.true.sink:
; CHECK-NEXT:    %a = getelementptr i8, ptr %p, i16 2
  %a = getelementptr i8, ptr %p, i16 2
  %r = select i1 %c, ptr %a, ptr %q
  ret ptr %r
}

define i16 @move_extend(i8 %x, i16 %y, i1 %c) {
; CHECK-LABEL: @move_extend(
; CHECK:       select.true.sink:
; CHECK-NEXT:    %a = zext i8 %x to i16
  %a = zext i8 %x to i16
  %r = select i1 %c, i16 %a, i16 %y
  ret i16 %r
}

define i8 @byte_op(i8 %x, i8 %y, i1 %c) {
; CHECK-LABEL: @byte_op(
; CHECK:       select.true.sink:
; CHECK-NEXT:    %a = add i8 %x, 7
  %a = add i8 %x, 7
  %r = select i1 %c, i8 %a, i8 %y
  ret i8 %r
}

define i8 @false_side(i8 %x, i8 %y, i1 %c) {
; CHECK-LABEL: @false_side(
; CHECK:       select.false.sink:
; CHECK-NEXT:    %a = and i8 %x, 15
  %a = and i8 %x, 15
  %r = select i1 %c, i8 %y, i8 %a
  ret i8 %r
}

define i16 @pair_register_operand(i16 %x, i16 %y, i16 %z, i1 %c) {
; CHECK-LABEL: @pair_register_operand(
; CHECK:       select.true.sink:
; CHECK-NEXT:    %a = xor i16 %x, %z
  %a = xor i16 %x, %z
  %r = select i1 %c, i16 %a, i16 %y
  ret i16 %r
}

define i16 @pair_every_byte_changed(i16 %x, i16 %y, i1 %c) {
; CHECK-LABEL: @pair_every_byte_changed(
; CHECK:       select.true.sink:
; CHECK-NEXT:    %a = xor i16 %x, 4660
  %a = xor i16 %x, 4660
  %r = select i1 %c, i16 %a, i16 %y
  ret i16 %r
}

; No byte is shared: the constant changes the low byte, and a carry out of the
; low byte can change the high byte.
define i16 @pair_add_low_byte(i16 %x, i16 %y, i1 %c) {
; CHECK-LABEL: @pair_add_low_byte(
; CHECK:       select.true.sink:
; CHECK-NEXT:    %a = add i16 %x, 5
  %a = add i16 %x, 5
  %r = select i1 %c, i16 %a, i16 %y
  ret i16 %r
}

define i16 @pair_shift(i16 %x, i16 %y, i1 %c) {
; CHECK-LABEL: @pair_shift(
; CHECK:       select.true.sink:
; CHECK-NEXT:    %a = shl i16 %x, 1
  %a = shl i16 %x, 1
  %r = select i1 %c, i16 %a, i16 %y
  ret i16 %r
}

define i16 @keep_xor_zero_byte(i16 %x, i1 %c) {
; CHECK-LABEL: @keep_xor_zero_byte(
; CHECK-NEXT:    %a = xor i16 %x, 4608
; CHECK-NOT:   sink
; CHECK:       ret i16
  %a = xor i16 %x, 4608
  %r = select i1 %c, i16 %a, i16 %x
  ret i16 %r
}

define i16 @keep_or_zero_byte(i16 %x, i1 %c) {
; CHECK-LABEL: @keep_or_zero_byte(
; CHECK-NEXT:    %a = or i16 %x, 128
; CHECK-NOT:   sink
; CHECK:       ret i16
  %a = or i16 %x, 128
  %r = select i1 %c, i16 %a, i16 %x
  ret i16 %r
}

define i16 @keep_and_ones_byte(i16 %x, i1 %c) {
; CHECK-LABEL: @keep_and_ones_byte(
; CHECK-NEXT:    %a = and i16 %x, -241
; CHECK-NOT:   sink
; CHECK:       ret i16
  %a = and i16 %x, -241
  %r = select i1 %c, i16 %a, i16 %x
  ret i16 %r
}

define i16 @keep_add_zero_low_byte(i16 %x, i1 %c) {
; CHECK-LABEL: @keep_add_zero_low_byte(
; CHECK-NEXT:    %a = add i16 %x, -256
; CHECK-NOT:   sink
; CHECK:       ret i16
  %a = add i16 %x, -256
  %r = select i1 %c, i16 %a, i16 %x
  ret i16 %r
}

define i16 @keep_sub_zero_low_byte(i16 %x, i1 %c) {
; CHECK-LABEL: @keep_sub_zero_low_byte(
; CHECK-NEXT:    %a = sub i16 %x, 512
; CHECK-NOT:   sink
; CHECK:       ret i16
  %a = sub i16 %x, 512
  %r = select i1 %c, i16 %a, i16 %x
  ret i16 %r
}

define i32 @keep_wide_zero_byte(i32 %x, i1 %c) {
; CHECK-LABEL: @keep_wide_zero_byte(
; CHECK-NEXT:    %a = xor i32 %x, 16711935
; CHECK-NOT:   sink
; CHECK:       ret i32
  %a = xor i32 %x, 16711935
  %r = select i1 %c, i32 %a, i32 %x
  ret i32 %r
}

define i16 @move_byte_not_shared(i16 %x, i16 %y, i1 %c) {
; CHECK-LABEL: @move_byte_not_shared(
; CHECK:       select.true.sink:
; CHECK-NEXT:    %a = xor i16 %x, 4608
  %a = xor i16 %x, 4608
  %r = select i1 %c, i16 %a, i16 %y
  ret i16 %r
}

define i16 @keep_both_arms_from_one_value(i16 %x, i1 %c) {
; CHECK-LABEL: @keep_both_arms_from_one_value(
; CHECK-NEXT:    %a = xor i16 %x, 4608
; CHECK-NEXT:    %b = add i16 %x, 768
; CHECK-NOT:   sink
; CHECK:       ret i16
  %a = xor i16 %x, 4608
  %b = add i16 %x, 768
  %r = select i1 %c, i16 %a, i16 %b
  ret i16 %r
}

; The add leaves the second byte as well, with no carry coming from the zero
; bytes below, so the arms share the second byte.
define i32 @keep_add_zero_lower_bytes(i32 %x, i1 %c) {
; CHECK-LABEL: @keep_add_zero_lower_bytes(
; CHECK-NEXT:    %a = add i32 %x, 65536
; CHECK-NEXT:    %b = xor i32 %x, 255
; CHECK-NOT:   sink
; CHECK:       ret i32
  %a = add i32 %x, 65536
  %b = xor i32 %x, 255
  %r = select i1 %c, i32 %a, i32 %b
  ret i32 %r
}
