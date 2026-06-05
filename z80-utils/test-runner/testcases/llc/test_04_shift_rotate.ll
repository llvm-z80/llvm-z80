; Test 04: shifts with various amounts (C rarely generates non-constant shifts)
; Tests G_SHL, G_LSHR, G_ASHR with constant and variable amounts
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

  ; Bit 0: shl i16 by 1 (0x0001 << 1 = 0x0002)
  %v0 = shl i16 1, 1
  %c0 = icmp eq i16 %v0, 2
  br i1 %c0, label %set0, label %t1
set0:
  %s0 = load i16, ptr %status
  store i16 1, ptr %status
  br label %t1
t1:

  ; Bit 1: shl i16 by 8 (0x00AB << 8 = 0xAB00)
  %v1 = shl i16 171, 8
  %c1 = icmp eq i16 %v1, -21760  ; 0xAB00
  br i1 %c1, label %set1, label %t2
set1:
  %s1 = load i16, ptr %status
  %s1a = or i16 %s1, 2
  store i16 %s1a, ptr %status
  br label %t2
t2:

  ; Bit 2: lshr i16 by 4 (0xFF00 >> 4 = 0x0FF0)
  %v2 = lshr i16 -256, 4   ; 0xFF00
  %c2 = icmp eq i16 %v2, 4080  ; 0x0FF0
  br i1 %c2, label %set2, label %t3
set2:
  %s2 = load i16, ptr %status
  %s2a = or i16 %s2, 4
  store i16 %s2a, ptr %status
  br label %t3
t3:

  ; Bit 3: ashr i16 by 4 (0xFF00 = -256 >> 4 = 0xFFF0 = -16)
  %v3 = ashr i16 -256, 4
  %c3 = icmp eq i16 %v3, -16
  br i1 %c3, label %set3, label %t4
set3:
  %s3 = load i16, ptr %status
  %s3a = or i16 %s3, 8
  store i16 %s3a, ptr %status
  br label %t4
t4:

  ; Bit 4: shl i8 by 3 (0x11 << 3 = 0x88)
  %v4 = shl i8 17, 3
  %c4 = icmp eq i8 %v4, -120  ; 0x88
  br i1 %c4, label %set4, label %t5
set4:
  %s4 = load i16, ptr %status
  %s4a = or i16 %s4, 16
  store i16 %s4a, ptr %status
  br label %t5
t5:

  ; Bit 5: ashr i8 by 7 (sign extension: -1 >> 7 = -1, 1 >> 7 = 0)
  %v5a = ashr i8 -1, 7
  %v5b = ashr i8 1, 7
  %c5a = icmp eq i8 %v5a, -1
  %c5b = icmp eq i8 %v5b, 0
  %c5 = and i1 %c5a, %c5b
  br i1 %c5, label %set5, label %t6
set5:
  %s5 = load i16, ptr %status
  %s5a = or i16 %s5, 32
  store i16 %s5a, ptr %status
  br label %t6
t6:

  ; Bit 6: shl i16 by 15 (only MSB matters)
  %v6 = shl i16 1, 15
  %c6 = icmp eq i16 %v6, -32768  ; 0x8000
  br i1 %c6, label %set6, label %t7
set6:
  %s6 = load i16, ptr %status
  %s6a = or i16 %s6, 64
  store i16 %s6a, ptr %status
  br label %t7
t7:

  ; Bit 7: lshr i16 by 15 (0x8000 >> 15 = 1)
  %v7 = lshr i16 -32768, 15
  %c7 = icmp eq i16 %v7, 1
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
