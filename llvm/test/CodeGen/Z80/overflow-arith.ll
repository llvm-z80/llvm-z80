; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s

declare {i32, i1} @llvm.sadd.with.overflow.i32(i32, i32)
declare {i32, i1} @llvm.ssub.with.overflow.i32(i32, i32)

; Test: i32 signed add with overflow detection
; G_SADDO is narrowed to i16 ops, G_SADDE handles the upper half carry chain.
; G_SADDE is lowered to add + XOR-based overflow detection.
define i32 @test_i32_sadd_overflow(i32 %a, i32 %b) {
; CHECK-LABEL: _test_i32_sadd_overflow:
; CHECK:       add hl,de
; CHECK:       ret
  %r = call {i32, i1} @llvm.sadd.with.overflow.i32(i32 %a, i32 %b)
  %val = extractvalue {i32, i1} %r, 0
  %ovf = extractvalue {i32, i1} %r, 1
  %res = select i1 %ovf, i32 0, i32 %val
  ret i32 %res
}

; Test: i32 signed sub with overflow detection
define i32 @test_i32_ssub_overflow(i32 %a, i32 %b) {
; CHECK-LABEL: _test_i32_ssub_overflow:
; CHECK:       sbc hl,
; CHECK:       ret
  %r = call {i32, i1} @llvm.ssub.with.overflow.i32(i32 %a, i32 %b)
  %val = extractvalue {i32, i1} %r, 0
  %ovf = extractvalue {i32, i1} %r, 1
  %res = select i1 %ovf, i32 0, i32 %val
  ret i32 %res
}
