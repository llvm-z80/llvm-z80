; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O0 < %s | FileCheck %s
; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O0 < %s | FileCheck %s --check-prefix=FP

; Test: large frame with FP uses IX + large-offset HL-indirect sequence
define i8 @large_frame_i8(i8 %val) "frame-pointer"="all" {
; FP-LABEL: _large_frame_i8:
; FP:       push ix
; FP:       push ix
; FP-NEXT:  pop hl
; FP-NEXT:  ld bc,
; FP-NEXT:  add hl,bc
; FP-NEXT:  ld (hl),a
; FP:       push ix
; FP-NEXT:  pop hl
; FP-NEXT:  ld bc,
; FP-NEXT:  add hl,bc
; FP-NEXT:  ld a,(hl)
  %arr = alloca [200 x i8], align 1
  store i8 %val, ptr %arr
  %v = load i8, ptr %arr
  ret i8 %v
}

; Test: small frame with FP uses IX+d directly
; The reload of A is eliminated by the late optimization pass because A
; already holds the stored value.
define i8 @small_frame_i8(i8 %val) "frame-pointer"="all" {
; FP-LABEL: _small_frame_i8:
; FP:       ld -4(ix),a
; FP-NOT:   ld a,-4(ix)
  %arr = alloca [4 x i8], align 1
  store i8 %val, ptr %arr
  %v = load i8, ptr %arr
  ret i8 %v
}

; Test: SP-relative addressing (no frame pointer)
define i8 @sp_relative_i8(i8 %val) {
; CHECK-LABEL: _sp_relative_i8:
; CHECK:       ld hl,#0
; CHECK-NEXT:  add hl,sp
; CHECK-NEXT:  ld (hl),a
; CHECK:       ld hl,#0
; CHECK-NEXT:  add hl,sp
; CHECK-NEXT:  ld a,(hl)
  %arr = alloca [4 x i8], align 1
  store i8 %val, ptr %arr
  %v = load i8, ptr %arr
  ret i8 %v
}
