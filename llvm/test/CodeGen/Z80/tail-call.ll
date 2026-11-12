; RUN: llc -mtriple=z80 -O1 -verify-machineinstrs < %s | FileCheck %s --check-prefixes=CHECK,Z80
; RUN: llc -mtriple=sm83 -O1 -verify-machineinstrs < %s | FileCheck %s --check-prefixes=CHECK,SM83
; RUN: llc -mtriple=z80 -O1 -filetype=obj < %s -o /dev/null
; RUN: llc -mtriple=sm83 -O1 -filetype=obj < %s -o /dev/null

; A call in tail position becomes a jump when each stack argument of the callee
; is the incoming argument from the same slot and both functions pop the same
; bytes off the stack.

declare void @v()
declare i16 @g(i16, i16)
declare i16 @g3(i16, i16, i16)
declare i16 @g4(i16, i16, i16, i16)
declare i16 @g_i32(i16, i16, i32)
declare i8 @g_i8(i8, i8, i8)
declare cc128 i16 @g3_cc0(i16, i16, i16)
declare cc128 i16 @g2_cc0(i16, i16)
declare cc129 i16 @g3_smallc(i16, i16, i16)
declare cc129 i16 @g2_smallc(i16, i16)
declare cc129 i8 @g_i8_smallc(i8, i8, i8)
declare cc128 i16 @cc0()
declare cc128 void @cc0v()
declare void @f(...)
declare void @llvm.memcpy.p0.p0.i16(ptr, ptr, i16, i1)
declare void @u(i8)
declare void @fa()
declare void @fb()
declare void @fc()
declare i16 @w()
@flag = external global i8
@buf = external global [300 x i8]

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

; The return is copied into the block with the call.
; CHECK-LABEL: _cond:
; CHECK-NOT:   call
; CHECK:       jp nz,_v
define void @cond(i16 %a) {
entry:
  %c = icmp eq i16 %a, 0
  br i1 %c, label %end, label %then
then:
  tail call void @v()
  br label %end
end:
  ret void
}

; CHECK-LABEL: _either:
; CHECK:       cp 3
; CHECK-NEXT:  jp nz,_u
; CHECK-NEXT:  {{.*}}:
; CHECK-NEXT:  jp _v
define void @either(i8 %a) {
entry:
  %c = icmp eq i8 %a, 3
  br i1 %c, label %t, label %e
t:
  tail call void @v()
  ret void
e:
  tail call void @u(i8 %a)
  ret void
}

; CHECK-LABEL: _dispatch:
; CHECK:       cp 7
; CHECK-NEXT:  jp z,_fc
; CHECK:       cp 2
; CHECK-NEXT:  jp z,_fb
; CHECK:       cp 1
; CHECK-NEXT:  jp z,_fa
define void @dispatch(i8 %x) optsize {
entry:
  switch i8 %x, label %def [ i8 1, label %a
                             i8 2, label %b
                             i8 7, label %c ]
a:
  tail call void @fa()
  ret void
b:
  tail call void @fb()
  ret void
c:
  tail call void @fc()
  ret void
def:
  store volatile i8 0, ptr @flag
  ret void
}

; Another path reaches the tail call. JP cc adds a byte, which only optsize
; refuses.
; CHECK-LABEL: _shared_os:
; CHECK:       jr z,
; CHECK:       jp _v
define void @shared_os(i8 %x, i8 %y) optsize {
entry:
  %c = icmp eq i8 %x, 0
  br i1 %c, label %call, label %mid
mid:
  store volatile i8 %y, ptr @flag
  br label %call
call:
  tail call void @v()
  ret void
}

; CHECK-LABEL: _shared:
; CHECK:       jp z,_v
; CHECK:       jp _v
define void @shared(i8 %x, i8 %y) {
entry:
  %c = icmp eq i8 %x, 0
  br i1 %c, label %call, label %mid
mid:
  store volatile i8 %y, ptr @flag
  br label %call
call:
  tail call void @v()
  ret void
}

; The call is rarely taken. JP cc would slow down the common path.
; CHECK-LABEL: _rare:
; CHECK:       jr z,
; CHECK:       jp _v
define void @rare(i8 %x) {
entry:
  %c = icmp eq i8 %x, 0
  br i1 %c, label %call, label %other, !prof !0
call:
  tail call void @v()
  ret void
other:
  store volatile i8 %x, ptr @flag
  ret void
}

; Three branches to the tail call leave the size unchanged. The call must then
; be likely even at optsize.
; CHECK-LABEL: _three:
; CHECK-COUNT-3: jp z,_v
define void @three(i8 %x, i8 %y) optsize {
entry:
  %a = icmp eq i8 %x, 0
  br i1 %a, label %call, label %b
b:
  %c = icmp eq i8 %y, 0
  br i1 %c, label %call, label %d
d:
  %z = load volatile i8, ptr @flag
  %e = icmp eq i8 %z, 0
  br i1 %e, label %call, label %done
call:
  tail call void @v()
  ret void
done:
  store volatile i8 0, ptr @flag
  ret void
}

; CHECK-LABEL: _three_rare:
; CHECK-COUNT-3: jr z,
; CHECK:       jp _v
define void @three_rare(i8 %x, i8 %y) optsize {
entry:
  %a = icmp eq i8 %x, 0
  br i1 %a, label %call, label %b, !prof !0
b:
  %c = icmp eq i8 %y, 0
  br i1 %c, label %call, label %d, !prof !0
d:
  %z = load volatile i8, ptr @flag
  %e = icmp eq i8 %z, 0
  br i1 %e, label %call, label %done, !prof !0
call:
  tail call void @v()
  ret void
done:
  store volatile i8 0, ptr @flag
  ret void
}

; The loop also reaches the tail call. A conditional tail call in the loop
; would stop block placement from rotating the loop.
; CHECK-LABEL: _loop_exit:
; CHECK-NOT:   jp {{.*}},_w
; CHECK:       jp _w
define i16 @loop_exit(i16 %n) optsize {
entry:
  %e = icmp ult i16 %n, 300
  br i1 %e, label %body, label %end
cond:
  %inc = add nuw nsw i16 %i, 1
  %done = icmp eq i16 %inc, 300
  br i1 %done, label %end, label %body
body:
  %i = phi i16 [ %inc, %cond ], [ %n, %entry ]
  %a = getelementptr inbounds nuw i8, ptr @buf, i16 %i
  %x = load i8, ptr %a
  %z = icmp eq i8 %x, 0
  br i1 %z, label %cond, label %out
end:
  %r = tail call i16 @w()
  br label %out
out:
  %p = phi i16 [ %r, %end ], [ 0, %body ]
  ret i16 %p
}

; The result reaches the return through a phi.
; CHECK-LABEL: _cond_value:
; CHECK-NOT:   call
; CHECK:       jp _g
define i16 @cond_value(i16 %a, i16 %b) {
entry:
  %c = icmp eq i16 %a, 0
  br i1 %c, label %end, label %then
then:
  %r = tail call i16 @g(i16 %b, i16 %a)
  br label %end
end:
  %p = phi i16 [ %r, %then ], [ 0, %entry ]
  ret i16 %p
}

; The third argument goes on the stack.
; CHECK-LABEL: _cond_stack_arg:
; CHECK:       call _g3
; CHECK-NEXT:  ret
define i16 @cond_stack_arg(i16 %a, i16 %b) {
entry:
  %c = icmp eq i16 %a, 0
  br i1 %c, label %end, label %then
then:
  %r = tail call i16 @g3(i16 %b, i16 %a, i16 3)
  br label %end
end:
  %p = phi i16 [ %r, %then ], [ 0, %entry ]
  ret i16 %p
}

; CHECK-LABEL: _cond_isr:
; CHECK:       call _v
; CHECK:       reti
; CHECK-NOT:   reti
define void @cond_isr() "interrupt" {
entry:
  %f = load volatile i8, ptr @flag
  %c = icmp eq i8 %f, 0
  br i1 %c, label %end, label %then
then:
  tail call void @v()
  br label %end
end:
  ret void
}

; CHECK-LABEL: _cond_disabled:
; CHECK:       call _v
; CHECK:       ret
; CHECK-NOT:   ret
define void @cond_disabled(i16 %a) "disable-tail-calls"="true" {
entry:
  %c = icmp eq i16 %a, 0
  br i1 %c, label %end, label %then
then:
  tail call void @v()
  br label %end
end:
  ret void
}

; The epilogue goes in front of the jump and leaves HL alone, since HL holds
; the first argument on Z80.
; CHECK-LABEL: _frame:
; Z80:         ld e,l
; Z80-NEXT:    ld d,h
; Z80-NEXT:    ld iy,8
; Z80-NEXT:    add iy,sp
; Z80-NEXT:    ld sp,iy
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

; CHECK-LABEL: _pass_through:
; CHECK:       %bb.0:
; CHECK-NEXT:  jp _g3
define i16 @pass_through(i16 %a, i16 %b, i16 %c) {
  %r = tail call i16 @g3(i16 %a, i16 %b, i16 %c)
  ret i16 %r
}

; CHECK-LABEL: _pass_through_regs:
; CHECK-NOT:   call
; CHECK:       jp _g3
define i16 @pass_through_regs(i16 %a, i16 %b, i16 %c) {
  %r = tail call i16 @g3(i16 %b, i16 %a, i16 %c)
  ret i16 %r
}

; CHECK-LABEL: _pass_through_i32:
; CHECK:       %bb.0:
; CHECK-NEXT:  jp _g_i32
define i16 @pass_through_i32(i16 %a, i16 %b, i32 %c) {
  %r = tail call i16 @g_i32(i16 %a, i16 %b, i32 %c)
  ret i16 %r
}

; CHECK-LABEL: _pass_through_i8:
; CHECK:       %bb.0:
; CHECK-NEXT:  jp _g_i8
define i8 @pass_through_i8(i8 %a, i8 %b, i8 %c) {
  %r = tail call i8 @g_i8(i8 %a, i8 %b, i8 %c)
  ret i8 %r
}

; __sdcccall(0) passes every argument on the stack. The caller pops the
; arguments.
; CHECK-LABEL: _pass_through_cc0:
; CHECK:       %bb.0:
; CHECK-NEXT:  jp _g3_cc0
define cc128 i16 @pass_through_cc0(i16 %a, i16 %b, i16 %c) {
  %r = tail call cc128 i16 @g3_cc0(i16 %a, i16 %b, i16 %c)
  ret i16 %r
}

; CHECK-LABEL: _pass_through_cc0_fewer:
; CHECK:       %bb.0:
; CHECK-NEXT:  jp _g2_cc0
define cc128 i16 @pass_through_cc0_fewer(i16 %a, i16 %b, i16 %c) {
  %r = tail call cc128 i16 @g2_cc0(i16 %a, i16 %b)
  ret i16 %r
}

; CHECK-LABEL: _pass_through_smallc:
; CHECK:       %bb.0:
; CHECK-NEXT:  jp _g3_smallc
define cc129 i16 @pass_through_smallc(i16 %a, i16 %b, i16 %c) {
  %r = tail call cc129 i16 @g3_smallc(i16 %a, i16 %b, i16 %c)
  ret i16 %r
}

; __smallc gives each i8 a 2-byte slot.
; CHECK-LABEL: _pass_through_smallc_i8:
; CHECK:       %bb.0:
; CHECK-NEXT:  jp _g_i8_smallc
define cc129 i8 @pass_through_smallc_i8(i8 %a, i8 %b, i8 %c) {
  %r = tail call cc129 i8 @g_i8_smallc(i8 %a, i8 %b, i8 %c)
  ret i8 %r
}

; __smallc pushes the first argument first. Dropping the first argument keeps
; the other arguments in place.
; CHECK-LABEL: _pass_through_smallc_last:
; CHECK:       %bb.0:
; CHECK-NEXT:  jp _g2_smallc
define cc129 i16 @pass_through_smallc_last(i16 %a, i16 %b, i16 %c) {
  %r = tail call cc129 i16 @g2_smallc(i16 %b, i16 %c)
  ret i16 %r
}

; CHECK-LABEL: _swapped_smallc:
; CHECK:       call _g2_smallc
define cc129 i16 @swapped_smallc(i16 %a, i16 %b) {
  %r = tail call cc129 i16 @g2_smallc(i16 %b, i16 %a)
  ret i16 %r
}

; CHECK-LABEL: _changed_stack_arg:
; CHECK:       call _g3
define i16 @changed_stack_arg(i16 %a, i16 %b, i16 %c) {
  %c1 = add i16 %c, 1
  %r = tail call i16 @g3(i16 %a, i16 %b, i16 %c1)
  ret i16 %r
}

; CHECK-LABEL: _swapped_stack_args:
; CHECK:       call _g4
define i16 @swapped_stack_args(i16 %a, i16 %b, i16 %c, i16 %d) {
  %r = tail call i16 @g4(i16 %a, i16 %b, i16 %d, i16 %c)
  ret i16 %r
}

; The volatile store changes the slot the third argument came from.
; CHECK-LABEL: _byval_written:
; CHECK:       call _g3
define i16 @byval_written(i16 %a, i16 %b, ptr byval(i16) %s) {
  %c = load i16, ptr %s
  store volatile i16 7, ptr %s
  %r = tail call i16 @g3(i16 %a, i16 %b, i16 %c)
  ret i16 %r
}

; This function pops the third argument. The variadic callee leaves the
; argument for the caller to pop.
; CHECK-LABEL: _pop_mismatch:
; CHECK:       call _f
define void @pop_mismatch(i16 %a, i16 %b, i16 %c) {
  tail call void (...) @f(i16 %c)
  ret void
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

!0 = !{!"branch_weights", i32 1, i32 1000}
