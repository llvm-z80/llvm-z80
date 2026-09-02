; RUN: llc -mtriple=z80 -O1 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=z80 -O0 -verify-machineinstrs < %s -o /dev/null

; Register outputs stored through a pointer (clang's "+g" and "=X" forms)
; are rewritten into direct register outputs with an explicit store, since
; the GlobalISel lowering has no store-back for indirect register outputs.

; CHECK-LABEL: readwrite:
define void @readwrite(ptr %p) {
  call void asm sideeffect "inc $0", "=*imr,0"(ptr elementtype(i16) %p, i16 41)
  ret void
}

; CHECK-LABEL: anyout:
define i16 @anyout() {
  %t = alloca i16, align 1
  call void asm sideeffect "ld $0, #7", "=*X"(ptr elementtype(i16) %t)
  %v = load i16, ptr %t, align 1
  ret i16 %v
}

; Mixed direct and indirect outputs: the direct result keeps its position
; and the indirect one becomes a second struct element that is stored.
; CHECK-LABEL: mixed:
define i16 @mixed(i16 %a, ptr %p) {
  %r = call i16 asm "nop", "=r,=*imr,r,1"(ptr elementtype(i16) %p, i16 %a, i16 1)
  ret i16 %r
}
