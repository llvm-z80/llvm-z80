; Test 14: saturating arithmetic (uadd.sat, usub.sat, sadd.sat, ssub.sat)
; Rust's saturating_add/sub generate these intrinsics
; expect 0x000F

define void @_start() {
  call void asm sideeffect "ld sp, #0xFFFE", ""()
  %r = call i16 @main()
  call void asm sideeffect ".globl _halt\0A_halt:\0Ahalt", ""()
  ret void
}

declare i16 @llvm.uadd.sat.i16(i16, i16)
declare i16 @llvm.usub.sat.i16(i16, i16)
declare i16 @llvm.sadd.sat.i16(i16, i16)
declare i16 @llvm.ssub.sat.i16(i16, i16)

define i16 @main() {
  %status = alloca i16
  store i16 0, ptr %status

  ; Bit 0: uadd.sat — 65000 + 1000 saturates to 65535
  %r0 = call i16 @llvm.uadd.sat.i16(i16 65000, i16 1000)
  %c0 = icmp eq i16 %r0, -1  ; 65535
  br i1 %c0, label %set0, label %t1
set0:
  %s0 = load i16, ptr %status
  %s0a = or i16 %s0, 1
  store i16 %s0a, ptr %status
  br label %t1
t1:

  ; Bit 1: usub.sat — 100 - 200 saturates to 0
  %r1 = call i16 @llvm.usub.sat.i16(i16 100, i16 200)
  %c1 = icmp eq i16 %r1, 0
  br i1 %c1, label %set1, label %t2
set1:
  %s1 = load i16, ptr %status
  %s1a = or i16 %s1, 2
  store i16 %s1a, ptr %status
  br label %t2
t2:

  ; Bit 2: sadd.sat — 30000 + 10000 saturates to 32767
  %r2 = call i16 @llvm.sadd.sat.i16(i16 30000, i16 10000)
  %c2 = icmp eq i16 %r2, 32767
  br i1 %c2, label %set2, label %t3
set2:
  %s2 = load i16, ptr %status
  %s2a = or i16 %s2, 4
  store i16 %s2a, ptr %status
  br label %t3
t3:

  ; Bit 3: ssub.sat — -30000 - 10000 saturates to -32768
  %r3 = call i16 @llvm.ssub.sat.i16(i16 -30000, i16 10000)
  %c3 = icmp eq i16 %r3, -32768
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
