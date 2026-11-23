; RUN: llc -verify-machineinstrs -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s --check-prefix=Z80
; RUN: llc -verify-machineinstrs -mtriple=sm83 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s --check-prefix=SM83

; Test: interrupt handler saves registers and returns with reti
; Z80 enables interrupts right before reti. SM83 reti enables them.

@g = global i8 0
@w = global i16 0
@pp = global ptr null

declare void @use(ptr)

define void @isr() #0 {
; Z80-LABEL: _isr:
; Z80-NOT:   ei
; Z80:       push af
; Z80:       push bc
; Z80:       pop af
; Z80-NEXT:  ei
; Z80-NEXT:  reti

; SM83-LABEL: _isr:
; SM83-NOT:   ei
; SM83:       push af
; SM83:       push bc
; SM83:       pop af
; SM83-NEXT:  reti
  store volatile i8 66, ptr @g
  ret void
}

; Test: non-interrupt function uses ret (not reti)
define void @normal() {
; Z80-LABEL: _normal:
; Z80-NOT:   reti
; Z80:       ret

; SM83-LABEL: _normal:
; SM83-NOT:   reti
; SM83:       ret
  store volatile i8 66, ptr @g
  ret void
}

; Test: interrupt handler with more register pressure saves more pairs
define void @isr_complex() #0 {
; Z80-LABEL: _isr_complex:
; Z80:       push af
; Z80:       push bc
; Z80:       reti

; SM83-LABEL: _isr_complex:
; SM83:       push af
; SM83:       push bc
; SM83:       reti
  %a = load volatile i8, ptr @g
  %b = add i8 %a, 1
  store volatile i8 %b, ptr @g
  %c = load volatile i8, ptr @g
  %d = add i8 %c, 2
  store volatile i8 %d, ptr @g
  %e = load volatile i8, ptr @g
  %f = add i8 %e, 3
  store volatile i8 %f, ptr @g
  ret void
}

; Test: interrupt handler saves AF when only the flags change
define void @isr_flags_only() #0 {
; Z80-LABEL: _isr_flags_only:
; Z80:       push af
; Z80:       add hl,bc
; Z80:       pop af
; Z80:       reti

; SM83-LABEL: _isr_flags_only:
; SM83:       push af
; SM83:       add hl,bc
; SM83:       pop af
; SM83:       reti
  %v = load volatile i16, ptr @w
  %s = add i16 %v, 300
  store volatile i16 %s, ptr @w
  ret void
}

; Test: interrupt handler that never saves AF leaves A alone
define void @isr_pointer_chase() #0 {
; Z80-LABEL: _isr_pointer_chase:
; Z80-NOT:   ld a,
; Z80:       reti

; SM83-LABEL: _isr_pointer_chase:
; SM83-NOT:   ld a,
; SM83:       reti
  %p = load volatile ptr, ptr @pp
  %q = load volatile ptr, ptr %p
  %v = load volatile i16, ptr %q
  store volatile i16 %v, ptr @w
  ret void
}

; Test: nested interrupt handler enables interrupts before saving registers
; and returns with a plain reti
define void @isr_nested() "interrupt"="nested" {
; Z80-LABEL: _isr_nested:
; Z80:       ; %bb.0:
; Z80-NEXT:  ei
; Z80-NEXT:  push af
; Z80:       pop af
; Z80-NEXT:  reti

; SM83-LABEL: _isr_nested:
; SM83:       ; %bb.0:
; SM83-NEXT:  ei
; SM83-NEXT:  push af
; SM83:       pop af
; SM83-NEXT:  reti
  store volatile i8 66, ptr @g
  ret void
}

; Test: interrupt handler saves AF before the frame setup changes F, and the
; frame setup leaves the unsaved HL alone
define void @isr_big_frame() #0 {
; Z80-LABEL: _isr_big_frame:
; Z80:       ; %bb.0:
; Z80-NEXT:  push af
; Z80-NOT:   ld sp,hl
; Z80:       reti

; SM83-LABEL: _isr_big_frame:
; SM83:       ; %bb.0:
; SM83-NEXT:  push af
; SM83-NEXT:  add sp,#-100
; SM83:       add sp,#100
; SM83-NEXT:  pop af
; SM83-NEXT:  reti
  %buf = alloca [100 x i8]
  %p = getelementptr [100 x i8], ptr %buf, i16 0, i16 50
  store volatile i8 0, ptr %p
  ret void
}

; Test: interrupt handler sets the frame pointer up after saving registers,
; and releases the frame before restoring them
define void @isr_frame_pointer() #1 {
; Z80-LABEL: _isr_frame_pointer:
; Z80:       ; %bb.0:
; Z80-NEXT:  push af
; Z80:       push iy
; Z80-NEXT:  push ix
; Z80-NEXT:  ld ix,#0
; Z80-NEXT:  add ix,sp
; Z80-NEXT:  push af
; Z80-NEXT:  push af
; Z80-NEXT:  push ix
; Z80-NEXT:  pop hl
; Z80-NEXT:  ld bc,#65532
; Z80:       call _use
; Z80-NEXT:  ld sp,ix
; Z80-NEXT:  pop ix
; Z80-NEXT:  pop iy
; Z80:       pop af
; Z80-NEXT:  ei
; Z80-NEXT:  reti

; SM83-LABEL: _isr_frame_pointer:
; SM83:       push af
; SM83:       add sp,#-4
; SM83:       add sp,#4
; SM83:       pop af
; SM83-NEXT:  reti
  %buf = alloca [4 x i8]
  call void @use(ptr %buf)
  ret void
}

; Test: interrupt handler without locals sets no frame pointer up
define void @isr_frame_pointer_no_locals() #1 {
; Z80-LABEL: _isr_frame_pointer_no_locals:
; Z80-NOT:   add ix,sp
; Z80:       reti
  store volatile i8 66, ptr @g
  ret void
}

attributes #0 = { "interrupt" }
attributes #1 = { "interrupt" "frame-pointer"="all" }
