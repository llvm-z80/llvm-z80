; Test 22: i8 saturating arithmetic (flag-based optimization)
; Tests uadd.sat, usub.sat, sadd.sat, ssub.sat for i8
; expect 0x000F

define void @_start() {
  call void asm sideeffect "ld sp, #0xFFFE", ""()
  %r = call i16 @main()
  call void asm sideeffect ".globl _halt\0A_halt:\0Ahalt", ""()
  ret void
}

declare i8 @llvm.uadd.sat.i8(i8, i8)
declare i8 @llvm.usub.sat.i8(i8, i8)
declare i8 @llvm.sadd.sat.i8(i8, i8)
declare i8 @llvm.ssub.sat.i8(i8, i8)

define i16 @main() {
  %status = alloca i16
  store i16 0, ptr %status

  ; Bit 0: uadd.sat(200, 100) should saturate to 255
  %r0 = call i8 @llvm.uadd.sat.i8(i8 200, i8 100)
  %c0 = icmp eq i8 %r0, -1  ; 255
  br i1 %c0, label %set0, label %t1
set0:
  %s0 = load i16, ptr %status
  %s0a = or i16 %s0, 1
  store i16 %s0a, ptr %status
  br label %t1
t1:

  ; Bit 1: usub.sat(50, 100) should saturate to 0
  %r1 = call i8 @llvm.usub.sat.i8(i8 50, i8 100)
  %c1 = icmp eq i8 %r1, 0
  br i1 %c1, label %set1, label %t2
set1:
  %s1 = load i16, ptr %status
  %s1a = or i16 %s1, 2
  store i16 %s1a, ptr %status
  br label %t2
t2:

  ; Bit 2: sadd.sat(100, 100) should saturate to 127
  %r2 = call i8 @llvm.sadd.sat.i8(i8 100, i8 100)
  %c2 = icmp eq i8 %r2, 127
  br i1 %c2, label %set2, label %t3
set2:
  %s2 = load i16, ptr %status
  %s2a = or i16 %s2, 4
  store i16 %s2a, ptr %status
  br label %t3
t3:

  ; Bit 3: ssub.sat(-100, 100) should saturate to -128
  %r3 = call i8 @llvm.ssub.sat.i8(i8 -100, i8 100)
  %c3 = icmp eq i8 %r3, -128
  br i1 %c3, label %set3, label %done
set3:
  %s3 = load i16, ptr %status
  %s3a = or i16 %s3, 8
  store i16 %s3a, ptr %status
  br label %done
done:
  %ret = load i16, ptr %status
  ret i16 %ret
}
