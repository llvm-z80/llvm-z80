; RUN: %llc %s -o %t.s
; RUN: %cc %t.s -o %t
; RUN: %run --expect-result=0x03FF %t

; Bit counts, bitreverse and unsigned multiply overflow on odd integer widths

@v16 = global [3 x i16] zeroinitializer
@v32 = global [4 x i32] zeroinitializer
@v64 = global i64 0

define i16 @main() {
  %p0 = getelementptr [3 x i16], ptr @v16, i16 0, i16 0
  %p1 = getelementptr [3 x i16], ptr @v16, i16 0, i16 1
  %p2 = getelementptr [3 x i16], ptr @v16, i16 0, i16 2
  %q0 = getelementptr [4 x i32], ptr @v32, i16 0, i16 0
  %q1 = getelementptr [4 x i32], ptr @v32, i16 0, i16 1
  %q2 = getelementptr [4 x i32], ptr @v32, i16 0, i16 2
  %q3 = getelementptr [4 x i32], ptr @v32, i16 0, i16 3
  store volatile i16 2748, ptr %p0          ; 0xABC
  store volatile i16 240, ptr %p1           ; 0x0F0
  store volatile i16 0, ptr %p2
  store volatile i32 3, ptr %q0
  store volatile i32 4096, ptr %q1          ; 0x1000
  store volatile i32 4095, ptr %q2          ; 0xFFF
  store volatile i32 160, ptr %q3           ; 0x0A0
  store volatile i64 1048576, ptr @v64      ; 0x100000
  %w0 = load volatile i16, ptr %p0
  %w1 = load volatile i16, ptr %p1
  %w2 = load volatile i16, ptr %p2
  %d0 = load volatile i32, ptr %q0
  %d1 = load volatile i32, ptr %q1
  %d2 = load volatile i32, ptr %q2
  %d3 = load volatile i32, ptr %q3
  %e0 = load volatile i64, ptr @v64

  ; Bit 0: ctpop i12 0xABC = 7
  %a12 = trunc i16 %w0 to i12
  %c0 = call i12 @llvm.ctpop.i12(i12 %a12)
  %ok0 = icmp eq i12 %c0, 7

  ; Bit 1: ctlz i12 0x0F0 = 4
  %b12 = trunc i16 %w1 to i12
  %c1 = call i12 @llvm.ctlz.i12(i12 %b12, i1 false)
  %ok1 = icmp eq i12 %c1, 4

  ; Bit 2: cttz i17 0x0A0 = 5
  %t17 = trunc i32 %d3 to i17
  %c2 = call i17 @llvm.cttz.i17(i17 %t17, i1 false)
  %ok2 = icmp eq i17 %c2, 5

  ; Bit 3: ctlz i12 0 = 12
  %z12 = trunc i16 %w2 to i12
  %c3 = call i12 @llvm.ctlz.i12(i12 %z12, i1 false)
  %ok3 = icmp eq i12 %c3, 12

  ; Bit 4: cttz i12 0 = 12
  %c4 = call i12 @llvm.cttz.i12(i12 %z12, i1 false)
  %ok4 = icmp eq i12 %c4, 12

  ; Bit 5: bitreverse i31 3 = 0x60000000
  %r31 = trunc i32 %d0 to i31
  %c5 = call i31 @llvm.bitreverse.i31(i31 %r31)
  %ok5 = icmp eq i31 %c5, 1610612736

  ; Bits 6, 7: umul i24 0x1000 * 0x1000 overflows to 0
  %x24 = trunc i32 %d1 to i24
  %m0 = call { i24, i1 } @llvm.umul.with.overflow.i24(i24 %x24, i24 %x24)
  %m0r = extractvalue { i24, i1 } %m0, 0
  %ok6 = extractvalue { i24, i1 } %m0, 1
  %ok7 = icmp eq i24 %m0r, 0

  ; Bit 8: umul i24 0xFFF * 0xFFF = 0xFFE001 without overflow
  %y24 = trunc i32 %d2 to i24
  %m1 = call { i24, i1 } @llvm.umul.with.overflow.i24(i24 %y24, i24 %y24)
  %m1r = extractvalue { i24, i1 } %m1, 0
  %m1o = extractvalue { i24, i1 } %m1, 1
  %m1v = icmp eq i24 %m1r, 16769025
  %m1n = xor i1 %m1o, true
  %ok8 = and i1 %m1v, %m1n

  ; Bit 9: umul i40 0x100000 * 0x100000 overflows
  %x40 = trunc i64 %e0 to i40
  %m2 = call { i40, i1 } @llvm.umul.with.overflow.i40(i40 %x40, i40 %x40)
  %ok9 = extractvalue { i40, i1 } %m2, 1

  %f0 = zext i1 %ok0 to i16
  %f1 = zext i1 %ok1 to i16
  %f2 = zext i1 %ok2 to i16
  %f3 = zext i1 %ok3 to i16
  %f4 = zext i1 %ok4 to i16
  %f5 = zext i1 %ok5 to i16
  %f6 = zext i1 %ok6 to i16
  %f7 = zext i1 %ok7 to i16
  %f8 = zext i1 %ok8 to i16
  %f9 = zext i1 %ok9 to i16
  %g1 = shl i16 %f1, 1
  %g2 = shl i16 %f2, 2
  %g3 = shl i16 %f3, 3
  %g4 = shl i16 %f4, 4
  %g5 = shl i16 %f5, 5
  %g6 = shl i16 %f6, 6
  %g7 = shl i16 %f7, 7
  %g8 = shl i16 %f8, 8
  %g9 = shl i16 %f9, 9
  %m01 = or i16 %f0, %g1
  %m02 = or i16 %m01, %g2
  %m03 = or i16 %m02, %g3
  %m04 = or i16 %m03, %g4
  %m05 = or i16 %m04, %g5
  %m06 = or i16 %m05, %g6
  %m07 = or i16 %m06, %g7
  %m08 = or i16 %m07, %g8
  %m09 = or i16 %m08, %g9
  ret i16 %m09
}
