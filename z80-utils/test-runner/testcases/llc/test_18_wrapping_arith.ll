; Test 18: wrapping arithmetic patterns (Rust wrapping_mul, wrapping_shl)
; Tests multiply and shifts that wrap around, common in Rust
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

  ; Bit 0: wrapping mul — 1000 * 100 = 100000 → wraps to 34464 (0x86A0)
  %a0 = add i16 1000, 0
  %b0 = add i16 100, 0
  %r0 = mul i16 %a0, %b0
  %c0 = icmp eq i16 %r0, -31072  ; 34464 = 0x86A0, as signed = -31072
  br i1 %c0, label %set0, label %t1
set0:
  %s0 = load i16, ptr %status
  %s0a = or i16 %s0, 1
  store i16 %s0a, ptr %status
  br label %t1
t1:

  ; Bit 1: wrapping shl — 1 << 15 = 0x8000 = -32768
  %a1 = add i16 1, 0
  %r1 = shl i16 %a1, 15
  %c1 = icmp eq i16 %r1, -32768
  br i1 %c1, label %set1, label %t2
set1:
  %s1 = load i16, ptr %status
  %s1a = or i16 %s1, 2
  store i16 %s1a, ptr %status
  br label %t2
t2:

  ; Bit 2: wrapping neg — 0 - MIN_I16 wraps to MIN_I16
  %a2 = add i16 -32768, 0
  %r2 = sub i16 0, %a2
  %c2 = icmp eq i16 %r2, -32768
  br i1 %c2, label %set2, label %t3
set2:
  %s2 = load i16, ptr %status
  %s2a = or i16 %s2, 4
  store i16 %s2a, ptr %status
  br label %t3
t3:

  ; Bit 3: wrapping add chain — 0xFFFF + 1 = 0
  %a3 = add i16 -1, 0  ; 0xFFFF
  %r3 = add i16 %a3, 1
  %c3 = icmp eq i16 %r3, 0
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
