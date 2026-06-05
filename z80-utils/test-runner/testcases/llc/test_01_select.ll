; Test 01: select instruction (conditional move without branch)
; C rarely generates select for complex conditions — tests G_SELECT lowering
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

  ; Bit 0: select i16 true
  %v0 = select i1 true, i16 42, i16 99
  %c0 = icmp eq i16 %v0, 42
  br i1 %c0, label %set0, label %next1
set0:
  %s0 = load i16, ptr %status
  %s0a = or i16 %s0, 1
  store i16 %s0a, ptr %status
  br label %next1
next1:

  ; Bit 1: select i16 false
  %v1 = select i1 false, i16 42, i16 99
  %c1 = icmp eq i16 %v1, 99
  br i1 %c1, label %set1, label %next2
set1:
  %s1 = load i16, ptr %status
  %s1a = or i16 %s1, 2
  store i16 %s1a, ptr %status
  br label %next2
next2:

  ; Bit 2: select i8 with runtime condition
  %x = add i8 3, 4
  %cond2 = icmp ugt i8 %x, 5
  %v2 = select i1 %cond2, i8 10, i8 20
  %c2 = icmp eq i8 %v2, 10
  br i1 %c2, label %set2, label %next3
set2:
  %s2 = load i16, ptr %status
  %s2a = or i16 %s2, 4
  store i16 %s2a, ptr %status
  br label %next3
next3:

  ; Bit 3: select i16 with computed condition
  %a = add i16 100, 200
  %b = add i16 150, 150
  %cond3 = icmp eq i16 %a, %b
  %v3 = select i1 %cond3, i16 1, i16 0
  %c3 = icmp eq i16 %v3, 1
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
