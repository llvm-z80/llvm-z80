; Test 17: byte swap and bit reverse
; Rust's swap_bytes/reverse_bits generate these
; expect 0x000F

define void @_start() {
  call void asm sideeffect "ld sp, #0xFFFE", ""()
  %r = call i16 @main()
  call void asm sideeffect ".globl _halt\0A_halt:\0Ahalt", ""()
  ret void
}

declare i16 @llvm.bswap.i16(i16)
declare i16 @llvm.bitreverse.i16(i16)

define i16 @main() {
  %status = alloca i16
  store i16 0, ptr %status

  ; Bit 0: bswap(0x1234) = 0x3412
  %v0 = add i16 4660, 0  ; 0x1234
  %r0 = call i16 @llvm.bswap.i16(i16 %v0)
  %c0 = icmp eq i16 %r0, 13330  ; 0x3412
  br i1 %c0, label %set0, label %t1
set0:
  %s0 = load i16, ptr %status
  %s0a = or i16 %s0, 1
  store i16 %s0a, ptr %status
  br label %t1
t1:

  ; Bit 1: bswap(0x00FF) = 0xFF00
  %v1 = add i16 255, 0
  %r1 = call i16 @llvm.bswap.i16(i16 %v1)
  %c1 = icmp eq i16 %r1, -256  ; 0xFF00
  br i1 %c1, label %set1, label %t2
set1:
  %s1 = load i16, ptr %status
  %s1a = or i16 %s1, 2
  store i16 %s1a, ptr %status
  br label %t2
t2:

  ; Bit 2: bitreverse(0x8000) = 0x0001
  %v2 = add i16 -32768, 0  ; 0x8000
  %r2 = call i16 @llvm.bitreverse.i16(i16 %v2)
  %c2 = icmp eq i16 %r2, 1
  br i1 %c2, label %set2, label %t3
set2:
  %s2 = load i16, ptr %status
  %s2a = or i16 %s2, 4
  store i16 %s2a, ptr %status
  br label %t3
t3:

  ; Bit 3: bitreverse(0x0001) = 0x8000
  %v3 = add i16 1, 0
  %r3 = call i16 @llvm.bitreverse.i16(i16 %v3)
  %c3 = icmp eq i16 %r3, -32768  ; 0x8000
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
