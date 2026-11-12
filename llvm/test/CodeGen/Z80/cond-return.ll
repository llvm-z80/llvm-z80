; RUN: llc -mtriple=z80 -O2 -verify-machineinstrs < %s | FileCheck %s --check-prefixes=CHECK,Z80
; RUN: llc -mtriple=sm83 -O2 -verify-machineinstrs < %s | FileCheck %s --check-prefixes=CHECK,SM83
; RUN: llc -mtriple=z80 -O2 -filetype=obj < %s -o /dev/null
; RUN: llc -mtriple=sm83 -O2 -filetype=obj < %s -o /dev/null

; A conditional branch to a block with only a RET becomes RET cc. A branch
; over such a block becomes RET cc with the opposite condition.

declare void @g()
declare i16 @g16(i16)
declare i8 @llvm.sadd.sat.i8(i8, i8)
@flag = external global i8

; CHECK-LABEL: _to_ret:
; CHECK:       or a
; CHECK-NEXT:  ret z
; CHECK:       ld ({{hl|de}}),a
; CHECK:       ret
define void @to_ret(ptr %p) {
entry:
  %v = load i8, ptr %p
  %c = icmp eq i8 %v, 0
  br i1 %c, label %end, label %set
set:
  store i8 1, ptr %p
  br label %end
end:
  ret void
}

; CHECK-LABEL: _over_ret:
; CHECK:       add a,a
; CHECK-NEXT:  ret nc
; CHECK:       call _g
define void @over_ret(i8 %x) {
entry:
  %c = icmp slt i8 %x, 0
  br i1 %c, label %t, label %f
t:
  call void @g()
  ret void
f:
  ret void
}

; CHECK-LABEL: _early:
; CHECK:       or {{[dh]}}
; CHECK-NEXT:  ret nz
; CHECK:       sbc a,0
; CHECK-NEXT:  ret c
; CHECK:       call _g
define void @early(i16 %a, i16 %b) {
entry:
  %c = icmp ne i16 %a, 0
  br i1 %c, label %out, label %next
next:
  %d = icmp ult i16 %b, 4
  br i1 %d, label %out, label %work
work:
  call void @g()
  ret void
out:
  ret void
}

; The return value is in place before the RET cc.
; CHECK-LABEL: _value:
; SM83:        ld c,e
; SM83-NEXT:   ld b,d
; CHECK:       or a
; CHECK-NEXT:  ret z
define i16 @value(i8 %x, i16 %a) {
entry:
  %c = icmp eq i8 %x, 0
  br i1 %c, label %end, label %work
work:
  %r = call i16 @g16(i16 %a)
  %s = add i16 %r, 1
  br label %end
end:
  %p = phi i16 [ %a, %entry ], [ %s, %work ]
  ret i16 %p
}

; The epilogue stays in the return block.
; CHECK-LABEL: _frame:
; CHECK:       jr z,
; CHECK-NOT:   ret z
define void @frame(i8 %x) {
entry:
  %buf = alloca [4 x i8]
  store volatile i8 %x, ptr %buf
  %c = icmp eq i8 %x, 0
  br i1 %c, label %end, label %work
work:
  call void @g()
  br label %end
end:
  ret void
}

; CHECK-LABEL: _isr:
; CHECK:       jr z,
; CHECK:       reti
define void @isr() "interrupt" {
entry:
  %f = load volatile i8, ptr @flag
  %c = icmp eq i8 %f, 0
  br i1 %c, label %end, label %work
work:
  store volatile i8 0, ptr @flag
  br label %end
end:
  ret void
}

; The branches that Z80ExpandPseudo adds fold too.
; Z80-LABEL:   _sat:
; Z80:         add a,l
; Z80-NEXT:    ret po
define i8 @sat(i8 %a, i8 %b) {
  %r = call i8 @llvm.sadd.sat.i8(i8 %a, i8 %b)
  ret i8 %r
}

; CHECK-LABEL: _sdiv:
; CHECK:       bit 7,c
; CHECK-NEXT:  ret z
define i8 @sdiv(i8 %a, i8 %b) {
  %r = sdiv i8 %a, %b
  ret i8 %r
}
