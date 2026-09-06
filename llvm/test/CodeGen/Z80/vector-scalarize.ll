; RUN: llc -mtriple=z80 -O1 -verify-machineinstrs < %s -o /dev/null
; RUN: llc -mtriple=sm83 -O1 -verify-machineinstrs < %s -o /dev/null
; RUN: llc -mtriple=z80 -O0 -verify-machineinstrs < %s -o /dev/null
; RUN: llc -mtriple=z80 -O1 -verify-machineinstrs < %s | FileCheck %s

; Vector operations have no hardware support and legalize to scalar pieces:
; arithmetic scalarizes per element, loads/stores split into element
; accesses, element indexing goes through a byte-aligned stack temporary,
; and vectors cross the calling convention as their integer image.

@v8 = global <8 x i8> zeroinitializer, align 1
@v4 = global <4 x i16> zeroinitializer, align 1

; CHECK-LABEL: add8:
define void @add8(ptr %p, ptr %q) {
  %a = load <8 x i8>, ptr %p, align 1
  %b = load <8 x i8>, ptr %q, align 1
  %s = add <8 x i8> %a, %b
  store <8 x i8> %s, ptr @v8, align 1
  ret void
}

; A vector argument and return travel as integers of the same size.
; CHECK-LABEL: pass16:
define <2 x i8> @pass16(<2 x i8> %v) {
  %r = mul <2 x i8> %v, <i8 3, i8 5>
  ret <2 x i8> %r
}

; Dynamic element index goes through a stack temporary.
; CHECK-LABEL: extract_dyn:
define i16 @extract_dyn(i16 %i) {
  %v = load <4 x i16>, ptr @v4, align 1
  %e = extractelement <4 x i16> %v, i16 %i
  ret i16 %e
}

; Compare + select with i1 vector intermediates.
; CHECK-LABEL: vmax:
define void @vmax(ptr %p, ptr %q) {
  %a = load <4 x i16>, ptr %p, align 1
  %b = load <4 x i16>, ptr %q, align 1
  %c = icmp ugt <4 x i16> %a, %b
  %m = select <4 x i1> %c, <4 x i16> %a, <4 x i16> %b
  store <4 x i16> %m, ptr @v4, align 1
  ret void
}

; Non-power-of-2 element count (SROA produces these for struct copies).
; CHECK-LABEL: odd_count:
define void @odd_count(ptr %p, ptr %q) {
  %a = load volatile <5 x i16>, ptr %p, align 1
  store volatile <5 x i16> %a, ptr %q, align 1
  ret void
}

; Shuffle lowers to element moves.
; CHECK-LABEL: splat:
define void @splat(ptr %p) {
  %v = load <4 x i16>, ptr %p, align 1
  %s = shufflevector <4 x i16> %v, <4 x i16> poison, <4 x i32> zeroinitializer
  store <4 x i16> %s, ptr @v4, align 1
  ret void
}
