; RUN: llc -mtriple=z80 -O1 -verify-machineinstrs < %s | FileCheck %s --check-prefixes=CHECK,Z80
; RUN: llc -mtriple=sm83 -O1 -verify-machineinstrs < %s | FileCheck %s --check-prefixes=CHECK,SM83
; RUN: llc -mtriple=z80 -O1 -filetype=obj < %s -o /dev/null
; RUN: llc -mtriple=sm83 -O1 -filetype=obj < %s -o /dev/null

; A call in tail position becomes a jump when the callee takes every argument
; in registers and the caller returns with a plain RET.

declare void @v()
declare i16 @g(i16, i16)
declare i16 @g3(i16, i16, i16)
declare cc128 i16 @cc0()
declare cc128 void @cc0v()
declare void @f(...)
declare void @llvm.memcpy.p0.p0.i16(ptr, ptr, i16, i1)

; CHECK-LABEL: _direct:
; CHECK:       %bb.0:
; CHECK-NEXT:  jp _v
define void @direct() {
  tail call void @v()
  ret void
}

; CHECK-LABEL: _args:
; CHECK-NOT:   call
; CHECK:       jp _g
define i16 @args(i16 %a, i16 %b) {
  %r = tail call i16 @g(i16 %b, i16 %a)
  ret i16 %r
}

; The epilogue goes in front of the jump and leaves HL alone, since HL holds
; the first argument on Z80.
; CHECK-LABEL: _frame:
; Z80:         ld e,l
; Z80-NEXT:    ld d,h
; Z80-NEXT:    pop af
; Z80-NEXT:    pop af
; Z80-NEXT:    pop af
; Z80-NEXT:    pop af
; SM83:        add sp,8
; CHECK-NEXT:  jp _g
define i16 @frame(i16 %a) {
  %buf = alloca [8 x i8]
  %p = getelementptr i8, ptr %buf, i16 6
  store volatile i8 1, ptr %p
  %r = tail call i16 @g(i16 %a, i16 %a)
  ret i16 %r
}

; Indirect calls jump to the target directly instead of through the
; __call_iy / __call_hl trampolines.
; CHECK-LABEL: _indirect:
; CHECK-NOT:   call
; Z80:         pop iy
; Z80-NEXT:    jp (iy)
; SM83:        jp (hl)
define i16 @indirect(ptr %fp, i16 %a) {
  %r = tail call i16 %fp(i16 %a, i16 %a)
  ret i16 %r
}

; Z80 copies a constant-size block inline; SM83 calls the runtime helper.
; SM83-LABEL: _libcall:
; SM83:        ld hl,10
; SM83-NEXT:   jp ___z80_memcpy_builtin
define void @libcall(ptr %d, ptr %s) {
  tail call void @llvm.memcpy.p0.p0.i16(ptr %d, ptr %s, i16 10, i1 false)
  ret void
}

; The memcpy helper returns nothing, so the destination is returned here.
; SM83-LABEL: _libcall_ret:
; SM83:        call ___z80_memcpy_builtin
; SM83:        ret
define ptr @libcall_ret(ptr %d, ptr %s) {
  tail call void @llvm.memcpy.p0.p0.i16(ptr %d, ptr %s, i16 10, i1 false)
  ret ptr %d
}

; A runtime call becomes a jump as well when the result is returned as it is.
; CHECK-LABEL: _helper:
; CHECK:       %bb.0:
; CHECK-NEXT:  jp ___mulhi3
define i16 @helper(i16 %a, i16 %b) {
  %r = mul i16 %a, %b
  ret i16 %r
}

; Z80 divides bytes inline; SM83 calls the runtime.
; SM83-LABEL: _helper_i8:
; SM83:        %bb.0:
; SM83-NEXT:   jp ___udivqi3
define i8 @helper_i8(i8 %a, i8 %b) {
  %r = udiv i8 %a, %b
  ret i8 %r
}

; CHECK-LABEL: _helper_used:
; CHECK:       call ___mulhi3
define i16 @helper_used(i16 %a, i16 %b) {
  %r = mul i16 %a, %b
  %s = add i16 %r, 1
  ret i16 %s
}

; __sdcccall(0) returns i16 in HL on Z80 and DE on SM83, unlike the helper.
; CHECK-LABEL: _helper_cc0:
; CHECK:       call ___mulhi3
define cc128 i16 @helper_cc0(i16 %a, i16 %b) {
  %r = mul i16 %a, %b
  ret i16 %r
}

; Nothing is returned, so the callee's return registers do not matter.
; CHECK-LABEL: _void_cc0:
; CHECK:       %bb.0:
; CHECK-NEXT:  jp _cc0v
define void @void_cc0() {
  tail call cc128 void @cc0v()
  ret void
}

; The variadic arguments stay in place on the stack.
; CHECK-LABEL: _forward:
; Z80:         ld sp,ix
; Z80-NEXT:    pop ix
; CHECK:       jp _f
define void @forward(...) {
  musttail call void (...) @f(...)
  ret void
}

; The third argument goes on the stack.
; CHECK-LABEL: _stack_arg:
; CHECK:       call _g3
; CHECK-NEXT:  ret
define i16 @stack_arg(i16 %a, i16 %b) {
  %r = tail call i16 @g3(i16 %a, i16 %b, i16 3)
  ret i16 %r
}

; This function pops the third argument off the stack on return.
; CHECK-LABEL: _callee_cleanup:
; CHECK:       call _g
define i16 @callee_cleanup(i16 %a, i16 %b, i16 %c) {
  %r = tail call i16 @g(i16 %a, i16 %c)
  ret i16 %r
}

; CHECK-LABEL: _isr:
; CHECK:       call _v
; CHECK:       reti
define void @isr() "interrupt" {
  tail call void @v()
  ret void
}

; __sdcccall(0) returns i16 in HL on Z80 and DE on SM83, unlike the caller.
; CHECK-LABEL: _ret_mismatch:
; CHECK:       %bb.0:
; CHECK-NEXT:  call _cc0
define i16 @ret_mismatch() {
  %r = tail call cc128 i16 @cc0()
  ret i16 %r
}

; The 16-bit routine returns the quotient in a register pair, and the caller
; reads A.
; CHECK-LABEL: _sdiv_i8:
; CHECK:       call ___divhi3
define i8 @sdiv_i8(i8 %a, i8 %b) minsize {
  %r = sdiv i8 %a, %b
  ret i8 %r
}

; CHECK-LABEL: _disabled:
; CHECK:       %bb.0:
; CHECK-NEXT:  call _v
; CHECK-NEXT:  ret
define void @disabled() "disable-tail-calls"="true" {
  tail call void @v()
  ret void
}

; CHECK-LABEL: _not_tail:
; CHECK:       %bb.0:
; CHECK-NEXT:  call _v
; CHECK-NEXT:  ret
define void @not_tail() {
  call void @v()
  ret void
}
