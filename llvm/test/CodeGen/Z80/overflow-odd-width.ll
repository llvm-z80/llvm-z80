; RUN: llc -mtriple=z80 -O0 -verify-machineinstrs < %s -o /dev/null
; RUN: llc -mtriple=sm83 -O0 -verify-machineinstrs < %s -o /dev/null
; RUN: llc -mtriple=z80 -O2 -verify-machineinstrs < %s -o /dev/null
;
; An add or subtract with overflow of odd width is widened to a power of two
; before it is split into pairs. Split as it stood, it left a 1-bit piece on
; top, whose signed add with carry could not be lowered and whose extracts
; and merges grew into ever wider types.

define void @sadd17(ptr %pa, ptr %pb, ptr %pr, ptr %po) {
  %a = load i17, ptr %pa, align 1
  %b = load i17, ptr %pb, align 1
  %s = call { i17, i1 } @llvm.sadd.with.overflow.i17(i17 %a, i17 %b)
  %r = extractvalue { i17, i1 } %s, 0
  %o = extractvalue { i17, i1 } %s, 1
  store i17 %r, ptr %pr, align 1
  store i1 %o, ptr %po, align 1
  ret void
}

define void @ssub17(ptr %pa, ptr %pb, ptr %pr, ptr %po) {
  %a = load i17, ptr %pa, align 1
  %b = load i17, ptr %pb, align 1
  %s = call { i17, i1 } @llvm.ssub.with.overflow.i17(i17 %a, i17 %b)
  %r = extractvalue { i17, i1 } %s, 0
  %o = extractvalue { i17, i1 } %s, 1
  store i17 %r, ptr %pr, align 1
  store i1 %o, ptr %po, align 1
  ret void
}

define void @uadd17(ptr %pa, ptr %pb, ptr %pr, ptr %po) {
  %a = load i17, ptr %pa, align 1
  %b = load i17, ptr %pb, align 1
  %s = call { i17, i1 } @llvm.uadd.with.overflow.i17(i17 %a, i17 %b)
  %r = extractvalue { i17, i1 } %s, 0
  %o = extractvalue { i17, i1 } %s, 1
  store i17 %r, ptr %pr, align 1
  store i1 %o, ptr %po, align 1
  ret void
}

define void @usub17(ptr %pa, ptr %pb, ptr %pr, ptr %po) {
  %a = load i17, ptr %pa, align 1
  %b = load i17, ptr %pb, align 1
  %s = call { i17, i1 } @llvm.usub.with.overflow.i17(i17 %a, i17 %b)
  %r = extractvalue { i17, i1 } %s, 0
  %o = extractvalue { i17, i1 } %s, 1
  store i17 %r, ptr %pr, align 1
  store i1 %o, ptr %po, align 1
  ret void
}
