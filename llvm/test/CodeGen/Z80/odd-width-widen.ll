; RUN: llc -mtriple=z80 -O0 -verify-machineinstrs < %s -o /dev/null
; RUN: llc -mtriple=sm83 -O0 -verify-machineinstrs < %s -o /dev/null
; RUN: llc -mtriple=z80 -O2 -verify-machineinstrs < %s -o /dev/null
; RUN: llc -mtriple=sm83 -O2 -verify-machineinstrs < %s -o /dev/null
;
; Bit counts and bitreverse of a width that is not whole bytes, and an
; unsigned multiply with overflow of odd width, are widened before they are
; lowered.

define void @ctpop12(ptr %p, ptr %r) {
  %a = load i12, ptr %p, align 1
  %c = call i12 @llvm.ctpop.i12(i12 %a)
  store i12 %c, ptr %r, align 1
  ret void
}

define void @ctlz12(ptr %p, ptr %r) {
  %a = load i12, ptr %p, align 1
  %c = call i12 @llvm.ctlz.i12(i12 %a, i1 false)
  store i12 %c, ptr %r, align 1
  ret void
}

define void @ctlz12_zero_poison(ptr %p, ptr %r) {
  %a = load i12, ptr %p, align 1
  %c = call i12 @llvm.ctlz.i12(i12 %a, i1 true)
  store i12 %c, ptr %r, align 1
  ret void
}

define void @cttz17(ptr %p, ptr %r) {
  %a = load i17, ptr %p, align 1
  %c = call i17 @llvm.cttz.i17(i17 %a, i1 false)
  store i17 %c, ptr %r, align 1
  ret void
}

define void @bitreverse31(ptr %p, ptr %r) {
  %a = load i31, ptr %p, align 1
  %c = call i31 @llvm.bitreverse.i31(i31 %a)
  store i31 %c, ptr %r, align 1
  ret void
}

define void @umul24(ptr %pa, ptr %pb, ptr %pr, ptr %po) {
  %a = load i24, ptr %pa, align 1
  %b = load i24, ptr %pb, align 1
  %m = call { i24, i1 } @llvm.umul.with.overflow.i24(i24 %a, i24 %b)
  %r = extractvalue { i24, i1 } %m, 0
  %o = extractvalue { i24, i1 } %m, 1
  store i24 %r, ptr %pr, align 1
  store i1 %o, ptr %po, align 1
  ret void
}

define void @umul40(ptr %pa, ptr %pb, ptr %pr, ptr %po) {
  %a = load i40, ptr %pa, align 1
  %b = load i40, ptr %pb, align 1
  %m = call { i40, i1 } @llvm.umul.with.overflow.i40(i40 %a, i40 %b)
  %r = extractvalue { i40, i1 } %m, 0
  %o = extractvalue { i40, i1 } %m, 1
  store i40 %r, ptr %pr, align 1
  store i1 %o, ptr %po, align 1
  ret void
}
