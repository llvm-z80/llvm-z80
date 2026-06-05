; Test 16: bit counting intrinsics (ctlz, cttz, ctpop)
; Rust's leading_zeros/trailing_zeros/count_ones generate these
; expect 0x000F

define void @_start() {
  call void asm sideeffect "ld sp, #0xFFFE", ""()
  %r = call i16 @main()
  call void asm sideeffect ".globl _halt\0A_halt:\0Ahalt", ""()
  ret void
}

declare i16 @llvm.ctlz.i16(i16, i1)
declare i16 @llvm.cttz.i16(i16, i1)
declare i16 @llvm.ctpop.i16(i16)

define i16 @main() {
  %status = alloca i16
  store i16 0, ptr %status

  ; Bit 0: ctlz(0x0100) = 7 (leading zeros of 256)
  %v0 = add i16 256, 0
  %r0 = call i16 @llvm.ctlz.i16(i16 %v0, i1 false)
  %c0 = icmp eq i16 %r0, 7
  br i1 %c0, label %set0, label %t1
set0:
  %s0 = load i16, ptr %status
  %s0a = or i16 %s0, 1
  store i16 %s0a, ptr %status
  br label %t1
t1:

  ; Bit 1: cttz(0x0080) = 7 (trailing zeros of 128)
  %v1 = add i16 128, 0
  %r1 = call i16 @llvm.cttz.i16(i16 %v1, i1 false)
  %c1 = icmp eq i16 %r1, 7
  br i1 %c1, label %set1, label %t2
set1:
  %s1 = load i16, ptr %status
  %s1a = or i16 %s1, 2
  store i16 %s1a, ptr %status
  br label %t2
t2:

  ; Bit 2: ctpop(0xFF00) = 8
  %v2 = add i16 65280, 0
  %r2 = call i16 @llvm.ctpop.i16(i16 %v2)
  %c2 = icmp eq i16 %r2, 8
  br i1 %c2, label %set2, label %t3
set2:
  %s2 = load i16, ptr %status
  %s2a = or i16 %s2, 4
  store i16 %s2a, ptr %status
  br label %t3
t3:

  ; Bit 3: ctpop(0x5555) = 8 (alternating bits)
  %v3 = add i16 21845, 0  ; 0x5555
  %r3 = call i16 @llvm.ctpop.i16(i16 %v3)
  %c3 = icmp eq i16 %r3, 8
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
