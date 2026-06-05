; Test 23: shift optimizations (SWAP on SM83, RLCA for shift-by-7)
; Tests LSHR by 4, 5, 6, 7 with various inputs
; expect 0x001F (5 bits, all pass)

define void @_start() {
  call void asm sideeffect "ld sp, #0xFFFE", ""()
  %r = call i16 @main()
  call void asm sideeffect ".globl _halt\0A_halt:\0Ahalt", ""()
  ret void
}

define i16 @main() {
  %status = alloca i16
  store i16 0, ptr %status

  ; Bit 0: 0xAB >> 4 = 0x0A
  %r0 = lshr i8 171, 4   ; 0xAB
  %c0 = icmp eq i8 %r0, 10  ; 0x0A
  br i1 %c0, label %set0, label %t1
set0:
  %s0 = load i16, ptr %status
  %s0a = or i16 %s0, 1
  store i16 %s0a, ptr %status
  br label %t1
t1:

  ; Bit 1: 0xAB >> 5 = 0x05
  %r1 = lshr i8 171, 5   ; 0xAB
  %c1 = icmp eq i8 %r1, 5
  br i1 %c1, label %set1, label %t2
set1:
  %s1 = load i16, ptr %status
  %s1a = or i16 %s1, 2
  store i16 %s1a, ptr %status
  br label %t2
t2:

  ; Bit 2: 0xAB >> 6 = 0x02
  %r2 = lshr i8 171, 6   ; 0xAB
  %c2 = icmp eq i8 %r2, 2
  br i1 %c2, label %set2, label %t3
set2:
  %s2 = load i16, ptr %status
  %s2a = or i16 %s2, 4
  store i16 %s2a, ptr %status
  br label %t3
t3:

  ; Bit 3: 0xAB >> 7 = 0x01
  %r3 = lshr i8 171, 7   ; 0xAB
  %c3 = icmp eq i8 %r3, 1
  br i1 %c3, label %set3, label %t4
set3:
  %s3 = load i16, ptr %status
  %s3a = or i16 %s3, 8
  store i16 %s3a, ptr %status
  br label %t4
t4:

  ; Bit 4: 0x7F >> 7 = 0x00
  %r4 = lshr i8 127, 7   ; 0x7F
  %c4 = icmp eq i8 %r4, 0
  br i1 %c4, label %set4, label %done
set4:
  %s4 = load i16, ptr %status
  %s4a = or i16 %s4, 16
  store i16 %s4a, ptr %status
  br label %done
done:
  %ret = load i16, ptr %status
  ret i16 %ret
}
