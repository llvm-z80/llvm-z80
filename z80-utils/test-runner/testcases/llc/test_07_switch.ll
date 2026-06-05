; Test 07: switch instruction with multiple cases
; C generates switch but with limited patterns; tests jump table lowering
; expect 0x000F

define void @_start() {
  call void asm sideeffect "ld sp, #0xFFFE", ""()
  %r = call i16 @main()
  call void asm sideeffect ".globl _halt\0A_halt:\0Ahalt", ""()
  ret void
}

; Helper: maps input to output via switch
define i16 @classify(i16 %x) {
entry:
  switch i16 %x, label %def [
    i16 0, label %c0
    i16 1, label %c1
    i16 2, label %c2
    i16 5, label %c5
    i16 10, label %c10
    i16 100, label %c100
  ]
c0:
  ret i16 1000
c1:
  ret i16 1001
c2:
  ret i16 1002
c5:
  ret i16 1005
c10:
  ret i16 1010
c100:
  ret i16 1100
def:
  ret i16 9999
}

define i16 @main() {
  %status = alloca i16
  store i16 0, ptr %status

  ; Bit 0: switch case 0
  %r0 = call i16 @classify(i16 0)
  %c0 = icmp eq i16 %r0, 1000
  br i1 %c0, label %set0, label %t1
set0:
  %s0 = load i16, ptr %status
  store i16 1, ptr %status
  br label %t1
t1:

  ; Bit 1: switch case 5 (non-contiguous)
  %r1 = call i16 @classify(i16 5)
  %c1 = icmp eq i16 %r1, 1005
  br i1 %c1, label %set1, label %t2
set1:
  %s1 = load i16, ptr %status
  %s1a = or i16 %s1, 2
  store i16 %s1a, ptr %status
  br label %t2
t2:

  ; Bit 2: switch case 100 (large gap)
  %r2 = call i16 @classify(i16 100)
  %c2 = icmp eq i16 %r2, 1100
  br i1 %c2, label %set2, label %t3
set2:
  %s2 = load i16, ptr %status
  %s2a = or i16 %s2, 4
  store i16 %s2a, ptr %status
  br label %t3
t3:

  ; Bit 3: switch default (value 42 not in cases)
  %r3 = call i16 @classify(i16 42)
  %c3 = icmp eq i16 %r3, 9999
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
