; Test 05: i32 arithmetic (add, sub, mul, shifts)
; Tests multi-word operations that require register pair splitting
; expect 0x000F

define void @_start() {
  call void asm sideeffect "ld sp, #0xFFFE", ""()
  %r = call i16 @main()
  call void asm sideeffect ".globl _halt\0A_halt:\0Ahalt", ""()
  ret void
}

define i16 @main() {
  %status = alloca i16
  store i16 0, ptr %status

  ; Bit 0: i32 add (0x00010000 + 0x00020000 = 0x00030000)
  %a0 = add i32 65536, 131072
  %c0 = icmp eq i32 %a0, 196608
  br i1 %c0, label %set0, label %t1
set0:
  %s0 = load i16, ptr %status
  store i16 1, ptr %status
  br label %t1
t1:

  ; Bit 1: i32 sub with borrow (0x10000 - 1 = 0xFFFF)
  %a1 = sub i32 65536, 1
  %c1 = icmp eq i32 %a1, 65535
  br i1 %c1, label %set1, label %t2
set1:
  %s1 = load i16, ptr %status
  %s1a = or i16 %s1, 2
  store i16 %s1a, ptr %status
  br label %t2
t2:

  ; Bit 2: i32 comparison signed (negative < positive)
  %c2 = icmp slt i32 -1, 1
  br i1 %c2, label %set2, label %t3
set2:
  %s2 = load i16, ptr %status
  %s2a = or i16 %s2, 4
  store i16 %s2a, ptr %status
  br label %t3
t3:

  ; Bit 3: i32 shl by 16 then trunc high word
  %v3 = shl i32 42, 16   ; 0x002A0000
  %hi = lshr i32 %v3, 16
  %hi16 = trunc i32 %hi to i16
  %c3 = icmp eq i16 %hi16, 42
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
