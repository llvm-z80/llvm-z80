; Test 09: bitwise operations (and, or, xor, not) on i8 and i16
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

  ; Bit 0: AND i16
  %a0 = and i16 65535, 255    ; 0xFFFF & 0x00FF = 0x00FF
  %c0 = icmp eq i16 %a0, 255
  br i1 %c0, label %set0, label %t1
set0:
  %s0 = load i16, ptr %status
  %s0a = or i16 %s0, 1
  store i16 %s0a, ptr %status
  br label %t1
t1:

  ; Bit 1: OR i16
  %a1 = or i16 240, 15       ; 0xF0 | 0x0F = 0xFF
  %c1 = icmp eq i16 %a1, 255
  br i1 %c1, label %set1, label %t2
set1:
  %s1 = load i16, ptr %status
  %s1a = or i16 %s1, 2
  store i16 %s1a, ptr %status
  br label %t2
t2:

  ; Bit 2: XOR i16
  %a2 = xor i16 65535, 65280  ; 0xFFFF ^ 0xFF00 = 0x00FF
  %c2 = icmp eq i16 %a2, 255
  br i1 %c2, label %set2, label %t3
set2:
  %s2 = load i16, ptr %status
  %s2a = or i16 %s2, 4
  store i16 %s2a, ptr %status
  br label %t3
t3:

  ; Bit 3: XOR as NOT (xor -1)
  %v3 = add i16 0, 65280      ; 0xFF00
  %a3 = xor i16 %v3, -1       ; ~0xFF00 = 0x00FF
  %c3 = icmp eq i16 %a3, 255
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
