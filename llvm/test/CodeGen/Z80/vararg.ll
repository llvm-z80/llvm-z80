; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O0 < %s | FileCheck %s
; Test variadic function support (va_start, va_arg)

target datalayout = "e-m:o-p:16:8-i16:8-i32:8-i64:8-n8:16"

declare void @llvm.va_start.p0(ptr)
declare void @llvm.va_end.p0(ptr)

; Test that variadic callee receives first fixed arg from stack (not HL register)
define i16 @sum(i16 %count, ...) {
; CHECK-LABEL: _sum:
; CHECK:       push ix
; CHECK:       ld ix,#0
; CHECK:       add ix,sp
; For vararg functions, count is on stack at IX+4 (not in HL register):
; CHECK:       ld {{[a-l]}},4(ix)
; CHECK:       ld {{[a-l]}},5(ix)
; va_start computes IX+6 (first vararg address):
; CHECK:       push ix
; CHECK-NEXT:  pop hl
; CHECK:       ld bc,#6
; CHECK-NEXT:  add hl,bc
  %ap = alloca ptr, align 1
  call void @llvm.va_start.p0(ptr %ap)
  %total = alloca i16, align 1
  store i16 0, ptr %total, align 1
  %i = alloca i16, align 1
  store i16 0, ptr %i, align 1
  br label %loop

loop:
  %iv = load i16, ptr %i, align 1
  %cmp = icmp slt i16 %iv, %count
  br i1 %cmp, label %body, label %exit

body:
  %val = va_arg ptr %ap, i16
  %cur = load i16, ptr %total, align 1
  %sum = add i16 %cur, %val
  store i16 %sum, ptr %total, align 1
  %next = load i16, ptr %i, align 1
  %inc = add i16 %next, 1
  store i16 %inc, ptr %i, align 1
  br label %loop

exit:
  call void @llvm.va_end.p0(ptr %ap)
  %result = load i16, ptr %total, align 1
  ret i16 %result
}

; Test that variadic caller pushes all args to stack (not registers)
define i16 @caller() {
; CHECK-LABEL: _caller:
; All args pushed to stack for variadic call (right-to-left):
; CHECK:       ld hl,#30
; CHECK-NEXT:  push hl
; CHECK:       ld hl,#20
; CHECK-NEXT:  push hl
; CHECK:       ld hl,#10
; CHECK-NEXT:  push hl
; CHECK:       ld hl,#3
; CHECK-NEXT:  push hl
; CHECK:       call _sum
  %result = call i16 (i16, ...) @sum(i16 3, i16 10, i16 20, i16 30)
  ret i16 %result
}
