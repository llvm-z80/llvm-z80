; RUN: llc -mtriple=sm83 -O2 -stop-after=irtranslator < %s \
; RUN:   | FileCheck %s --check-prefix=MIR
; RUN: llc -verify-machineinstrs -mtriple=sm83 -O2 < %s \
; RUN:   | FileCheck %s --check-prefix=SM83
; RUN: llc -verify-machineinstrs -mtriple=z80 -O2 < %s -o /dev/null

; A byte argument pushed as PUSH AF + INC SP does not read the flags, so the
; frame accesses between the pushes need not save them.

; MIR-LABEL: name: scroll
; MIR:       PUSH_AF implicit $a, implicit undef $flags

; SM83-LABEL: _scroll:
; SM83-NOT:   pop af
; SM83:       ret

declare z80_sdcccall0 i8 @getpix(i8, i8)
declare z80_sdcccall0 void @color(i8, i8, i8)
declare z80_sdcccall0 void @plot_point(i8, i8)

define void @scroll() {
entry:
  br label %outer

outer:
  %b = phi i8 [ 0, %entry ], [ %b.next, %outer.latch ]
  %b.next = add i8 %b, 1
  br label %inner

inner:
  %a = phi i8 [ 0, %outer ], [ %a.next, %inner ]
  %px = call z80_sdcccall0 i8 @getpix(i8 %a, i8 %b.next)
  call z80_sdcccall0 void @color(i8 %px, i8 0, i8 0)
  call z80_sdcccall0 void @plot_point(i8 %a, i8 %b)
  %a.next = add i8 %a, 1
  %done = icmp eq i8 %a.next, 160
  br i1 %done, label %outer.latch, label %inner

outer.latch:
  %outer.done = icmp eq i8 %b.next, 143
  br i1 %outer.done, label %exit, label %outer

exit:
  ret void
}
