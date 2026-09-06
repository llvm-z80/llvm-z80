; RUN: llc -verify-machineinstrs -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s
;
; An 8-bit ALU operation can read its operand out of a frame slot, so a value
; that is only wanted there never needs a register. Whether the slot is
; reachable that way is settled after the frame is laid out, so selection
; leaves the addressing mode open and the expansion picks it.

declare void @sink(ptr)

define i8 @f_add(i8 %x) "frame-pointer"="all" {
  %v = alloca i8
  call void @sink(ptr %v)
  %a = load volatile i8, ptr %v
  %r = add i8 %x, %a
  ret i8 %r
}

define i8 @f_sub(i8 %x) "frame-pointer"="all" {
  %v = alloca i8
  call void @sink(ptr %v)
  %a = load volatile i8, ptr %v
  %r = sub i8 %x, %a
  ret i8 %r
}

define i8 @f_and(i8 %x) "frame-pointer"="all" {
  %v = alloca i8
  call void @sink(ptr %v)
  %a = load volatile i8, ptr %v
  %r = and i8 %x, %a
  ret i8 %r
}

define i8 @f_or(i8 %x) "frame-pointer"="all" {
  %v = alloca i8
  call void @sink(ptr %v)
  %a = load volatile i8, ptr %v
  %r = or i8 %x, %a
  ret i8 %r
}

define i8 @f_xor(i8 %x) "frame-pointer"="all" {
  %v = alloca i8
  call void @sink(ptr %v)
  %a = load volatile i8, ptr %v
  %r = xor i8 %x, %a
  ret i8 %r
}

; Without a frame pointer there is no IX+d to read the slot with, so the
; value goes through a register as before.

define i8 @no_frame_pointer(i8 %x) "frame-pointer"="none" {
  %v = alloca i8
  call void @sink(ptr %v)
  %a = load volatile i8, ptr %v
  %r = and i8 %x, %a
  ret i8 %r
}

; CHECK-LABEL: _f_add:
; CHECK:       add a,-1(ix)
; CHECK-LABEL: _f_sub:
; CHECK:       sub -1(ix)
; CHECK-LABEL: _f_and:
; CHECK:       and -1(ix)
; CHECK-LABEL: _f_or:
; CHECK:       or -1(ix)
; CHECK-LABEL: _f_xor:
; CHECK:       xor -1(ix)
; CHECK-LABEL: _no_frame_pointer:
; CHECK-NOT:   and -{{[0-9]+}}(ix)
; CHECK:       and b
