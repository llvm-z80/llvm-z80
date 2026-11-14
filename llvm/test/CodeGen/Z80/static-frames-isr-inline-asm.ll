; RUN: llc -mtriple=z80 -O1 -z80-static-frames -verify-machineinstrs < %s | FileCheck %s

; Issue #413: Inline assembly in an interrupt handler (such as 'ei', 'di',
; 'in', 'out') must not be treated as an indirect call to unknown code.
; In LLVM CallGraph, inline asm has no direct callee and defaults to an edge
; to CallsExternalNode. Z80NonReentrant must filter out inline asm edges so that
; foreground functions are not falsely marked as reachable from the ISR context.

@sink = global ptr null

; Shape (c) positive control: genuinely shared function called by both ISR
; and foreground code must keep its stack frame.
; CHECK-LABEL: shared:
; CHECK-NOT: L_shared.frame
define internal i16 @shared(i16 %x) {
  %buf = alloca i16, align 1
  store volatile i16 %x, ptr %buf, align 1
  %v = load volatile i16, ptr %buf, align 1
  ret i16 %v
}

; Shape (b) structural variation: helper called only by ISR, containing inline asm.
; CHECK-LABEL: isr_helper:
; CHECK: L_isr_helper.frame
define internal void @isr_helper() {
  %buf = alloca i16, align 1
  store volatile i16 42, ptr %buf, align 1
  tail call void asm sideeffect "in a, ($0)", "i"(i8 4)
  ret void
}

; Shape (a) exact pattern: ISR containing inline asm ('ei') and memory clobber.
; CHECK-LABEL: isr:
; CHECK: L_isr.frame
define void @isr() #0 {
  %buf = alloca i16, align 1
  store volatile i16 7, ptr %buf, align 1
  call void @isr_helper()
  %r = call i16 @shared(i16 3)
  tail call void asm sideeffect "ei", "~{memory}"()
  ret void
}

; Shape (b) structural variation: foreground helper containing inline asm.
; CHECK-LABEL: foreground_asm_helper:
; CHECK: L_foreground_asm_helper.frame
define internal i16 @foreground_asm_helper(i16 %x) {
  %buf = alloca i16, align 1
  store volatile i16 %x, ptr %buf, align 1
  tail call void asm sideeffect "nop", ""()
  %v = load volatile i16, ptr %buf, align 1
  ret i16 %v
}

; Shape (a) foreground main function: must receive a static frame despite the ISR using inline asm.
; CHECK-LABEL: main:
; CHECK: L_main.frame
define i16 @main() {
  %buf = alloca [4 x i16], align 1
  store volatile i16 1, ptr %buf, align 1
  %a = call i16 @foreground_asm_helper(i16 2)
  %b = call i16 @shared(i16 5)
  %r = add i16 %a, %b
  ret i16 %r
}

; Shape (d) safety check: frames are consolidated into L___static_frames.
; CHECK: L___static_frames
; CHECK-DAG: L_main.frame = L___static_frames
; CHECK-DAG: L_isr.frame = L___static_frames
; CHECK-DAG: L_isr_helper.frame = L___static_frames
; CHECK-DAG: L_foreground_asm_helper.frame = L___static_frames

attributes #0 = { "interrupt" }
