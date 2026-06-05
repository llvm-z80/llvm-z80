; Test 08: all ICMP predicates (eq, ne, ugt, uge, ult, ule, sgt, sge, slt, sle)
; C rarely tests all predicates in one function; thorough ISel coverage
; expect 0x03FF

define void @_start() {
  call void asm sideeffect "ld sp, #0xFFFE", ""()
  %r = call i16 @main()
  call void asm sideeffect ".globl _halt\0A_halt:\0Ahalt", ""()
  ret void
}

define i16 @main() {
  %status = alloca i16
  store i16 0, ptr %status

  %a = add i16 10, 0
  %b = add i16 20, 0
  %neg = sub i16 0, 5  ; -5

  ; Bit 0: eq (10 == 10)
  %c0 = icmp eq i16 %a, 10
  br i1 %c0, label %set0, label %t1
set0:
  %s0 = load i16, ptr %status
  store i16 1, ptr %status
  br label %t1
t1:

  ; Bit 1: ne (10 != 20)
  %c1 = icmp ne i16 %a, %b
  br i1 %c1, label %set1, label %t2
set1:
  %s1 = load i16, ptr %status
  %s1a = or i16 %s1, 2
  store i16 %s1a, ptr %status
  br label %t2
t2:

  ; Bit 2: ugt (20 >u 10)
  %c2 = icmp ugt i16 %b, %a
  br i1 %c2, label %set2, label %t3
set2:
  %s2 = load i16, ptr %status
  %s2a = or i16 %s2, 4
  store i16 %s2a, ptr %status
  br label %t3
t3:

  ; Bit 3: uge (10 >=u 10)
  %c3 = icmp uge i16 %a, 10
  br i1 %c3, label %set3, label %t4
set3:
  %s3 = load i16, ptr %status
  %s3a = or i16 %s3, 8
  store i16 %s3a, ptr %status
  br label %t4
t4:

  ; Bit 4: ult (10 <u 20)
  %c4 = icmp ult i16 %a, %b
  br i1 %c4, label %set4, label %t5
set4:
  %s4 = load i16, ptr %status
  %s4a = or i16 %s4, 16
  store i16 %s4a, ptr %status
  br label %t5
t5:

  ; Bit 5: ule (10 <=u 10)
  %c5 = icmp ule i16 %a, 10
  br i1 %c5, label %set5, label %t6
set5:
  %s5 = load i16, ptr %status
  %s5a = or i16 %s5, 32
  store i16 %s5a, ptr %status
  br label %t6
t6:

  ; Bit 6: sgt (10 >s -5)
  %c6 = icmp sgt i16 %a, %neg
  br i1 %c6, label %set6, label %t7
set6:
  %s6 = load i16, ptr %status
  %s6a = or i16 %s6, 64
  store i16 %s6a, ptr %status
  br label %t7
t7:

  ; Bit 7: sge (-5 >=s -5)
  %c7 = icmp sge i16 %neg, -5
  br i1 %c7, label %set7, label %t8
set7:
  %s7 = load i16, ptr %status
  %s7a = or i16 %s7, 128
  store i16 %s7a, ptr %status
  br label %t8
t8:

  ; Bit 8: slt (-5 <s 10)
  %c8 = icmp slt i16 %neg, %a
  br i1 %c8, label %set8, label %t9
set8:
  %s8 = load i16, ptr %status
  %s8a = or i16 %s8, 256
  store i16 %s8a, ptr %status
  br label %t9
t9:

  ; Bit 9: sle (10 <=s 10)
  %c9 = icmp sle i16 %a, 10
  br i1 %c9, label %set9, label %done
set9:
  %s9 = load i16, ptr %status
  %s9a = or i16 %s9, 512
  store i16 %s9a, ptr %status
  br label %done
done:
  %ret = load i16, ptr %status
  ret i16 %ret
}
