; Test 03: zero/sign extension patterns
; Tests G_ZEXT, G_SEXT, G_TRUNC across i1/i8/i16/i32
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

  ; Bit 0: zext i8 → i16 (0xFF → 0x00FF)
  %z0 = zext i8 255 to i16
  %c0 = icmp eq i16 %z0, 255
  br i1 %c0, label %set0, label %t1
set0:
  %s0 = load i16, ptr %status
  %s0a = or i16 %s0, 1
  store i16 %s0a, ptr %status
  br label %t1
t1:

  ; Bit 1: sext i8 → i16 (0xFF = -1 → 0xFFFF)
  %s1v = sext i8 -1 to i16
  %c1 = icmp eq i16 %s1v, -1
  br i1 %c1, label %set1, label %t2
set1:
  %s1 = load i16, ptr %status
  %s1a = or i16 %s1, 2
  store i16 %s1a, ptr %status
  br label %t2
t2:

  ; Bit 2: sext i8 → i16 (0x80 = -128 → 0xFF80)
  %s2v = sext i8 -128 to i16
  %c2 = icmp eq i16 %s2v, -128
  br i1 %c2, label %set2, label %t3
set2:
  %s2 = load i16, ptr %status
  %s2a = or i16 %s2, 4
  store i16 %s2a, ptr %status
  br label %t3
t3:

  ; Bit 3: sext i8 → i16 positive (0x7F = 127 → 0x007F)
  %s3v = sext i8 127 to i16
  %c3 = icmp eq i16 %s3v, 127
  br i1 %c3, label %set3, label %t4
set3:
  %s3 = load i16, ptr %status
  %s3a = or i16 %s3, 8
  store i16 %s3a, ptr %status
  br label %t4
t4:

  ; Bit 4: trunc i16 → i8 (0x1234 → 0x34)
  %tr4 = trunc i16 4660 to i8  ; 0x1234
  %c4 = icmp eq i8 %tr4, 52    ; 0x34
  br i1 %c4, label %set4, label %t5
set4:
  %s4 = load i16, ptr %status
  %s4a = or i16 %s4, 16
  store i16 %s4a, ptr %status
  br label %t5
t5:

  ; Bit 5: zext i1 → i16 (true → 1)
  %cmp5 = icmp ugt i16 10, 5
  %z5 = zext i1 %cmp5 to i16
  %c5 = icmp eq i16 %z5, 1
  br i1 %c5, label %set5, label %t6
set5:
  %s5 = load i16, ptr %status
  %s5a = or i16 %s5, 32
  store i16 %s5a, ptr %status
  br label %t6
t6:

  ; Bit 6: zext i1 → i8 (false → 0)
  %cmp6 = icmp ugt i16 5, 10
  %z6 = zext i1 %cmp6 to i8
  %c6 = icmp eq i8 %z6, 0
  br i1 %c6, label %set6, label %t7
set6:
  %s6 = load i16, ptr %status
  %s6a = or i16 %s6, 64
  store i16 %s6a, ptr %status
  br label %t7
t7:

  ; Bit 7: zext i8 → i32, trunc i32 → i16
  %z7 = zext i8 200 to i32
  %tr7 = trunc i32 %z7 to i16
  %c7 = icmp eq i16 %tr7, 200
  br i1 %c7, label %set7, label %done
set7:
  %s7 = load i16, ptr %status
  %s7a = or i16 %s7, 128
  store i16 %s7a, ptr %status
  br label %done
done:
  %ret = load i16, ptr %status
  ret i16 %ret
}
