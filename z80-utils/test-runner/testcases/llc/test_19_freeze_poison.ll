; Test 19: freeze instruction (Rust's MaybeUninit, safe undef handling)
; freeze converts poison/undef to an arbitrary but fixed value
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

  ; Bit 0: freeze of a concrete value is identity
  %v0 = add i16 42, 0
  %f0 = freeze i16 %v0
  %c0 = icmp eq i16 %f0, 42
  br i1 %c0, label %set0, label %t1
set0:
  %s0 = load i16, ptr %status
  %s0a = or i16 %s0, 1
  store i16 %s0a, ptr %status
  br label %t1
t1:

  ; Bit 1: freeze of computed value preserves it
  %a1 = add i16 100, 0
  %b1 = add i16 200, 0
  %sum1 = add i16 %a1, %b1
  %f1 = freeze i16 %sum1
  %c1 = icmp eq i16 %f1, 300
  br i1 %c1, label %set1, label %t2
set1:
  %s1 = load i16, ptr %status
  %s1a = or i16 %s1, 2
  store i16 %s1a, ptr %status
  br label %t2
t2:

  ; Bit 2: freeze of i8 value
  %v2 = add i8 99, 0
  %f2 = freeze i8 %v2
  %c2 = icmp eq i8 %f2, 99
  br i1 %c2, label %set2, label %t3
set2:
  %s2 = load i16, ptr %status
  %s2a = or i16 %s2, 4
  store i16 %s2a, ptr %status
  br label %t3
t3:

  ; Bit 3: freeze of boolean (i1)
  %cmp3 = icmp ugt i16 10, 5
  %f3 = freeze i1 %cmp3
  br i1 %f3, label %set3, label %done
set3:
  %s3 = load i16, ptr %status
  %s3a = or i16 %s3, 8
  store i16 %s3a, ptr %status
  br label %done
done:
  %ret = load i16, ptr %status
  ret i16 %ret
}
