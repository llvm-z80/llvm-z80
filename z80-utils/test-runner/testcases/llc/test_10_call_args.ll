; Test 10: function calls with multiple arguments (tests calling convention)
; expect 0x000F

define void @_start() {
  call void asm sideeffect "ld sp, #0xFFFE", ""()
  %r = call i16 @main()
  call void asm sideeffect ".globl _halt\0A_halt:\0Ahalt", ""()
  ret void
}

define i16 @add_i16(i16 %a, i16 %b) {
  %r = add i16 %a, %b
  ret i16 %r
}

define i16 @sub_three(i16 %a, i16 %b, i16 %c) {
  %t = sub i16 %a, %b
  %r = sub i16 %t, %c
  ret i16 %r
}

define i8 @add_i8(i8 %a, i8 %b) {
  %r = add i8 %a, %b
  ret i8 %r
}

define i16 @main() {
  %status = alloca i16
  store i16 0, ptr %status

  ; Bit 0: call with 2 i16 args
  %r0 = call i16 @add_i16(i16 100, i16 200)
  %c0 = icmp eq i16 %r0, 300
  br i1 %c0, label %set0, label %t1
set0:
  %s0 = load i16, ptr %status
  %s0a = or i16 %s0, 1
  store i16 %s0a, ptr %status
  br label %t1
t1:

  ; Bit 1: call with 3 i16 args (3rd goes on stack)
  %r1 = call i16 @sub_three(i16 1000, i16 300, i16 200)
  %c1 = icmp eq i16 %r1, 500
  br i1 %c1, label %set1, label %t2
set1:
  %s1 = load i16, ptr %status
  %s1a = or i16 %s1, 2
  store i16 %s1a, ptr %status
  br label %t2
t2:

  ; Bit 2: call with i8 args
  %r2 = call i8 @add_i8(i8 50, i8 70)
  %c2 = icmp eq i8 %r2, 120
  br i1 %c2, label %set2, label %t3
set2:
  %s2 = load i16, ptr %status
  %s2a = or i16 %s2, 4
  store i16 %s2a, ptr %status
  br label %t3
t3:

  ; Bit 3: nested calls
  %inner = call i16 @add_i16(i16 10, i16 20)
  %r3 = call i16 @add_i16(i16 %inner, i16 70)
  %c3 = icmp eq i16 %r3, 100
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
