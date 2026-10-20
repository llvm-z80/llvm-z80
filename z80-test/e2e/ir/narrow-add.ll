; RUN: %llc %s -o %t.s
; RUN: %cc %t.s -o %t
; RUN: %run --expect-result=0x003F %t

; Add and sub on integers narrower than a byte

@v = global [4 x i8] zeroinitializer

define i16 @main() {
  %pa = getelementptr [4 x i8], ptr @v, i16 0, i16 0
  %pb = getelementptr [4 x i8], ptr @v, i16 0, i16 1
  %pc = getelementptr [4 x i8], ptr @v, i16 0, i16 2
  %pd = getelementptr [4 x i8], ptr @v, i16 0, i16 3
  store volatile i8 1, ptr %pa
  store volatile i8 1, ptr %pb
  store volatile i8 5, ptr %pc
  store volatile i8 6, ptr %pd
  %a = load volatile i8, ptr %pa
  %b = load volatile i8, ptr %pb
  %c = load volatile i8, ptr %pc
  %d = load volatile i8, ptr %pd

  ; Bit 0: i1 1 + 1 = 0
  %a1 = trunc i8 %a to i1
  %b1 = trunc i8 %b to i1
  %s1 = add i1 %a1, %b1
  %z0 = zext i1 %s1 to i16
  %ok0 = icmp eq i16 %z0, 0

  ; Bit 1: i1 1 - 0 = 1
  %t1 = sub i1 %a1, %s1
  %z1 = zext i1 %t1 to i16
  %ok1 = icmp eq i16 %z1, 1

  ; Bit 2: i2 1 + 1 = 2
  %a2 = trunc i8 %a to i2
  %b2 = trunc i8 %b to i2
  %s2 = add i2 %a2, %b2
  %z2 = zext i2 %s2 to i16
  %ok2 = icmp eq i16 %z2, 2

  ; Bit 3: i3 5 + 6 = 3
  %c3 = trunc i8 %c to i3
  %d3 = trunc i8 %d to i3
  %s3 = add i3 %c3, %d3
  %z3 = zext i3 %s3 to i16
  %ok3 = icmp eq i16 %z3, 3

  ; Bit 4: i3 5 - 6 = 7
  %t3 = sub i3 %c3, %d3
  %z4 = zext i3 %t3 to i16
  %ok4 = icmp eq i16 %z4, 7

  ; Bit 5: i4 5 + 6 = 11
  %c4 = trunc i8 %c to i4
  %d4 = trunc i8 %d to i4
  %s4 = add i4 %c4, %d4
  %z5 = zext i4 %s4 to i16
  %ok5 = icmp eq i16 %z5, 11

  %f0 = zext i1 %ok0 to i16
  %f1 = zext i1 %ok1 to i16
  %f2 = zext i1 %ok2 to i16
  %f3 = zext i1 %ok3 to i16
  %f4 = zext i1 %ok4 to i16
  %f5 = zext i1 %ok5 to i16
  %g1 = shl i16 %f1, 1
  %g2 = shl i16 %f2, 2
  %g3 = shl i16 %f3, 3
  %g4 = shl i16 %f4, 4
  %g5 = shl i16 %f5, 5
  %m1 = or i16 %f0, %g1
  %m2 = or i16 %m1, %g2
  %m3 = or i16 %m2, %g3
  %m4 = or i16 %m3, %g4
  %m5 = or i16 %m4, %g5
  ret i16 %m5
}
