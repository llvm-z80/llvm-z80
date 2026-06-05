; Test 11: overflow detection (sadd.with.overflow, ssub.with.overflow, uadd.with.overflow)
; These intrinsics are rarely generated from C without __builtin_*
; expect 0x000F

define void @_start() {
  call void asm sideeffect "ld sp, #0xFFFE", ""()
  %r = call i16 @main()
  call void asm sideeffect ".globl _halt\0A_halt:\0Ahalt", ""()
  ret void
}

declare {i16, i1} @llvm.sadd.with.overflow.i16(i16, i16)
declare {i16, i1} @llvm.ssub.with.overflow.i16(i16, i16)
declare {i16, i1} @llvm.uadd.with.overflow.i16(i16, i16)

define i16 @main() {
  %status = alloca i16
  store i16 0, ptr %status

  ; Bit 0: sadd no overflow (100 + 200 = 300)
  %r0 = call {i16, i1} @llvm.sadd.with.overflow.i16(i16 100, i16 200)
  %v0 = extractvalue {i16, i1} %r0, 0
  %o0 = extractvalue {i16, i1} %r0, 1
  %ok0a = icmp eq i16 %v0, 300
  %ok0b = icmp eq i1 %o0, false
  %ok0 = and i1 %ok0a, %ok0b
  br i1 %ok0, label %set0, label %t1
set0:
  %s0 = load i16, ptr %status
  %s0a = or i16 %s0, 1
  store i16 %s0a, ptr %status
  br label %t1
t1:

  ; Bit 1: sadd with overflow (32000 + 32000 overflows i16 signed)
  %r1 = call {i16, i1} @llvm.sadd.with.overflow.i16(i16 32000, i16 32000)
  %o1 = extractvalue {i16, i1} %r1, 1
  br i1 %o1, label %set1, label %t2
set1:
  %s1 = load i16, ptr %status
  %s1a = or i16 %s1, 2
  store i16 %s1a, ptr %status
  br label %t2
t2:

  ; Bit 2: ssub with overflow (-30000 - 10000 overflows)
  %r2 = call {i16, i1} @llvm.ssub.with.overflow.i16(i16 -30000, i16 10000)
  %o2 = extractvalue {i16, i1} %r2, 1
  br i1 %o2, label %set2, label %t3
set2:
  %s2 = load i16, ptr %status
  %s2a = or i16 %s2, 4
  store i16 %s2a, ptr %status
  br label %t3
t3:

  ; Bit 3: uadd with overflow (65000 + 1000 overflows u16)
  %r3 = call {i16, i1} @llvm.uadd.with.overflow.i16(i16 65000, i16 1000)
  %o3 = extractvalue {i16, i1} %r3, 1
  br i1 %o3, label %set3, label %done
set3:
  %s3 = load i16, ptr %status
  %s3a = or i16 %s3, 8
  store i16 %s3a, ptr %status
  br label %done
done:
  %ret = load i16, ptr %status
  ret i16 %ret
}
