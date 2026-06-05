; Test 15: min/max/abs intrinsics
; Rust's min/max/abs and C23's stdbit.h generate these
; expect 0x00FF

define void @_start() {
  call void asm sideeffect "ld sp, #0xFFFE", ""()
  %r = call i16 @main()
  call void asm sideeffect ".globl _halt\0A_halt:\0Ahalt", ""()
  ret void
}

declare i16 @llvm.smin.i16(i16, i16)
declare i16 @llvm.smax.i16(i16, i16)
declare i16 @llvm.umin.i16(i16, i16)
declare i16 @llvm.umax.i16(i16, i16)
declare i16 @llvm.abs.i16(i16, i1)

define i16 @main() {
  %status = alloca i16
  store i16 0, ptr %status

  ; Bit 0: smin(100, -50) = -50
  %r0 = call i16 @llvm.smin.i16(i16 100, i16 -50)
  %c0 = icmp eq i16 %r0, -50
  br i1 %c0, label %set0, label %t1
set0:
  %s0 = load i16, ptr %status
  %s0a = or i16 %s0, 1
  store i16 %s0a, ptr %status
  br label %t1
t1:

  ; Bit 1: smax(100, -50) = 100
  %r1 = call i16 @llvm.smax.i16(i16 100, i16 -50)
  %c1 = icmp eq i16 %r1, 100
  br i1 %c1, label %set1, label %t2
set1:
  %s1 = load i16, ptr %status
  %s1a = or i16 %s1, 2
  store i16 %s1a, ptr %status
  br label %t2
t2:

  ; Bit 2: umin(100, 50000) = 100
  %r2 = call i16 @llvm.umin.i16(i16 100, i16 50000)
  %c2 = icmp eq i16 %r2, 100
  br i1 %c2, label %set2, label %t3
set2:
  %s2 = load i16, ptr %status
  %s2a = or i16 %s2, 4
  store i16 %s2a, ptr %status
  br label %t3
t3:

  ; Bit 3: umax(100, 50000) = 50000
  %r3 = call i16 @llvm.umax.i16(i16 100, i16 50000)
  %c3 = icmp eq i16 %r3, 50000
  br i1 %c3, label %set3, label %t4
set3:
  %s3 = load i16, ptr %status
  %s3a = or i16 %s3, 8
  store i16 %s3a, ptr %status
  br label %t4
t4:

  ; Bit 4: abs(-500) = 500
  %r4 = call i16 @llvm.abs.i16(i16 -500, i1 false)
  %c4 = icmp eq i16 %r4, 500
  br i1 %c4, label %set4, label %t5
set4:
  %s4 = load i16, ptr %status
  %s4a = or i16 %s4, 16
  store i16 %s4a, ptr %status
  br label %t5
t5:

  ; Bit 5: abs(500) = 500 (positive unchanged)
  %r5 = call i16 @llvm.abs.i16(i16 500, i1 false)
  %c5 = icmp eq i16 %r5, 500
  br i1 %c5, label %set5, label %t6
set5:
  %s5 = load i16, ptr %status
  %s5a = or i16 %s5, 32
  store i16 %s5a, ptr %status
  br label %t6
t6:

  ; Bit 6: smin with equal values
  %r6 = call i16 @llvm.smin.i16(i16 42, i16 42)
  %c6 = icmp eq i16 %r6, 42
  br i1 %c6, label %set6, label %t7
set6:
  %s6 = load i16, ptr %status
  %s6a = or i16 %s6, 64
  store i16 %s6a, ptr %status
  br label %t7
t7:

  ; Bit 7: umax(0, 1) = 1
  %r7 = call i16 @llvm.umax.i16(i16 0, i16 1)
  %c7 = icmp eq i16 %r7, 1
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
