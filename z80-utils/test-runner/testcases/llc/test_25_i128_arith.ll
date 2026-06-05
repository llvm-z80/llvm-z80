; Test 25: i128 arithmetic (add, sub, mul, udiv, urem, signed)
; Tests 128-bit operations via libcalls and narrowing
; expect 0x00FF

define void @_start() {
  call void asm sideeffect "ld sp, #0xFFFE", ""()
  %r = call i16 @main()
  call void asm sideeffect ".globl _halt\0A_halt:\0Ahalt", ""()
  ret void
}

define i16 @main() {
  %status = alloca i16
  store i16 0, ptr %status

  ; Bit 0: i128 add with carry propagation across 64-bit boundary
  ; 0xFFFFFFFFFFFFFFFF + 1 == 0x10000000000000000
  %a0_lo = zext i64 -1 to i128
  %b0 = zext i64 1 to i128
  %r0 = add i128 %a0_lo, %b0
  %expected0 = shl i128 1, 64
  %c0 = icmp eq i128 %r0, %expected0
  br i1 %c0, label %set0, label %t1
set0:
  %s0 = load i16, ptr %status
  %s0n = or i16 %s0, 1
  store i16 %s0n, ptr %status
  br label %t1
t1:

  ; Bit 1: i128 subtract across 64-bit boundary
  ; 0x10000000000000000 - 1 == 0xFFFFFFFFFFFFFFFF
  %a1 = shl i128 1, 64
  %r1 = sub i128 %a1, 1
  %expected1 = zext i64 -1 to i128
  %c1 = icmp eq i128 %r1, %expected1
  br i1 %c1, label %set1, label %t2
set1:
  %s1 = load i16, ptr %status
  %s1n = or i16 %s1, 2
  store i16 %s1n, ptr %status
  br label %t2
t2:

  ; Bit 2: i128 multiply: 12345 * 67890 == 838102050
  %a2 = zext i64 12345 to i128
  %b2 = zext i64 67890 to i128
  %r2 = mul i128 %a2, %b2
  %expected2 = zext i64 838102050 to i128
  %c2 = icmp eq i128 %r2, %expected2
  br i1 %c2, label %set2, label %t3
set2:
  %s2 = load i16, ptr %status
  %s2n = or i16 %s2, 4
  store i16 %s2n, ptr %status
  br label %t3
t3:

  ; Bit 3: i128 unsigned division: 838102050 / 12345 == 67890
  %a3 = zext i64 838102050 to i128
  %b3 = zext i64 12345 to i128
  %r3 = udiv i128 %a3, %b3
  %expected3 = zext i64 67890 to i128
  %c3 = icmp eq i128 %r3, %expected3
  br i1 %c3, label %set3, label %t4
set3:
  %s3 = load i16, ptr %status
  %s3n = or i16 %s3, 8
  store i16 %s3n, ptr %status
  br label %t4
t4:

  ; Bit 4: i128 unsigned modulo: 838102050 % 67890 == 838102050 - 12345*67890
  ; 838102050 % 67890 == 0 (exact multiple actually: 12345 * 67890 = 838102050)
  ; Let's use: 838102057 % 67890; 838102057 = 12345*67890 + 7, so mod = 7
  %a4 = zext i64 838102057 to i128
  %b4 = zext i64 67890 to i128
  %r4 = urem i128 %a4, %b4
  %expected4 = zext i64 7 to i128
  %c4 = icmp eq i128 %r4, %expected4
  br i1 %c4, label %set4, label %t5
set4:
  %s4 = load i16, ptr %status
  %s4n = or i16 %s4, 16
  store i16 %s4n, ptr %status
  br label %t5
t5:

  ; Bit 5: i128 signed add: (-1) + (-1) == -2
  %r5 = add i128 -1, -1
  %c5 = icmp eq i128 %r5, -2
  br i1 %c5, label %set5, label %t6
set5:
  %s5 = load i16, ptr %status
  %s5n = or i16 %s5, 32
  store i16 %s5n, ptr %status
  br label %t6
t6:

  ; Bit 6: i128 signed division: -100 / 7 == -14 (truncated toward zero)
  %r6 = sdiv i128 -100, 7
  %c6 = icmp eq i128 %r6, -14
  br i1 %c6, label %set6, label %t7
set6:
  %s6 = load i16, ptr %status
  %s6n = or i16 %s6, 64
  store i16 %s6n, ptr %status
  br label %t7
t7:

  ; Bit 7: i128 overflow wrap: 0xFFFF...FFFF + 1 == 0
  %r7 = add i128 -1, 1
  %c7 = icmp eq i128 %r7, 0
  br i1 %c7, label %set7, label %done
set7:
  %s7 = load i16, ptr %status
  %s7n = or i16 %s7, 128
  store i16 %s7n, ptr %status
  br label %done
done:

  %result = load i16, ptr %status
  ret i16 %result
}
