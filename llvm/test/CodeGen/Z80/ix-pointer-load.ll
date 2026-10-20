; RUN: llc -mtriple=z80 -O2 < %s | FileCheck %s
; RUN: llc -mtriple=z80 -O2 -stop-after=instruction-select < %s | FileCheck %s --check-prefix=MIR

; A pointer that an inline asm output pins to IX is read with LD r,(IX+d), and
; each byte load carries the part of the memory operand for that byte.

define i8 @byte() {
; CHECK-LABEL: _byte:
; CHECK:       ld a,(ix+4)
; MIR-LABEL:   name: byte
; MIR:         LD_r_IXd 4, implicit $ix :: (load (s8) from %ir.p)
  %fp = call ptr asm sideeffect "", "={ix}"()
  %p = getelementptr i8, ptr %fp, i16 4
  %v = load i8, ptr %p
  ret i8 %v
}

define i16 @pair() {
; CHECK-LABEL: _pair:
; CHECK:       ld e,(ix+-6)
; CHECK-NEXT:  ld d,(ix+-5)
; MIR-LABEL:   name: pair
; MIR:         LD_r_IXd -6, implicit $ix :: (volatile load (s8) from %ir.p)
; MIR-NEXT:    LD_r_IXd -5, implicit $ix :: (volatile load (s8) from %ir.p + 1)
  %fp = call ptr asm sideeffect "", "={ix}"()
  %p = getelementptr i8, ptr %fp, i16 -6
  %v = load volatile i16, ptr %p
  ret i16 %v
}

define i8 @no_offset() {
; CHECK-LABEL: _no_offset:
; CHECK:       ld a,(ix+0)
  %fp = call ptr asm sideeffect "", "={ix}"()
  %v = load i8, ptr %fp
  ret i8 %v
}

; Once a later asm writes IX, the pointer is read through another register.

define i8 @rewritten() {
; CHECK-LABEL: _rewritten:
; CHECK-NOT:   (ix+
; CHECK:       ret
  %p = call ptr asm sideeffect "", "={ix}"()
  %q = call ptr asm sideeffect "", "={ix}"()
  %v = load i8, ptr %p
  ret i8 %v
}

define i8 @clobbered() {
; CHECK-LABEL: _clobbered:
; CHECK-NOT:   (ix+
; CHECK:       ret
  %fp = call ptr asm sideeffect "", "={ix}"()
  call void asm sideeffect "", "~{ix}"()
  %p = getelementptr i8, ptr %fp, i16 1
  %v = load i8, ptr %p
  ret i8 %v
}
