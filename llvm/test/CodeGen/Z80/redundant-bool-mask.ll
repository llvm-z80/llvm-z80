; RUN: llc -verify-machineinstrs -mtriple=z80 -O1 -stop-before=instruction-select < %s | FileCheck %s --check-prefix=MIR
; RUN: llc -verify-machineinstrs -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s --check-prefix=Z80
;
; Widening an i1 branch condition to a byte leaves the condition masked with
; one. Over a comparison that mask says nothing, and while it stands the
; comparison is hidden from everything that looks for one.

declare i32 @src32()
declare i64 @src64()
declare i16 @src16()
declare void @taken()

; A wide comparison becomes a target opcode, so the mask only goes away if
; the target says what that opcode produces.

define void @eq32() {
entry:
  %x = call i32 @src32()
  %c = icmp eq i32 %x, 83810205
  br i1 %c, label %then, label %exit
then:
  call void @taken()
  br label %exit
exit:
  ret void
}

; MIR-LABEL: name: eq32
; MIR:      G_Z80_ICMP32
; MIR-NEXT: G_BRCOND

; With the comparison in plain sight the branch takes it directly, instead of
; each half becoming a boolean that is combined and then tested again.

; Z80-LABEL: _eq32:
; Z80-NOT:  sbc a,a
; Z80:      or c
; Z80-NEXT: jr nz,

define void @ne64() {
entry:
  %x = call i64 @src64()
  %c = icmp ne i64 %x, 1311768467463790320
  br i1 %c, label %then, label %exit
then:
  call void @taken()
  br label %exit
exit:
  ret void
}

; MIR-LABEL: name: ne64
; MIR:      G_Z80_ICMP64
; MIR-NEXT: G_BRCOND

; A comparison narrow enough to stay generic needs nothing from the target.

define void @ult16() {
entry:
  %x = call i16 @src16()
  %c = icmp ult i16 %x, 4660
  br i1 %c, label %then, label %exit
then:
  call void @taken()
  br label %exit
exit:
  ret void
}

; MIR-LABEL: name: ult16
; MIR:      G_ICMP
; MIR-NEXT: G_BRCOND

; A mask over anything else is doing its job and stays.

define i8 @mask_kept(i8 %x) {
  %a = and i8 %x, 1
  ret i8 %a
}

; MIR-LABEL: name: mask_kept
; MIR:      G_AND

; Z80-LABEL: _mask_kept:
; Z80:      and #1
