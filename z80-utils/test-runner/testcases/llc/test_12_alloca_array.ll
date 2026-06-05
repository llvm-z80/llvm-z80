; Test 12: stack-allocated arrays and pointer arithmetic
; Tests alloca with array sizes, GEP indexing, and store/load patterns
; expect 0x000F

define void @_start() {
  call void asm sideeffect "ld sp, #0xFFFE", ""()
  %r = call i16 @main()
  call void asm sideeffect ".globl _halt\0A_halt:\0Ahalt", ""()
  ret void
}

define i16 @main() {
  ; All allocas in entry block (static frame allocation)
  %status = alloca i16
  %arr = alloca [4 x i16]
  %barr = alloca [8 x i8]
  %single = alloca i16
  store i16 0, ptr %status

  ; Bit 0: alloca array of 4 i16, store and load back
  %p0 = getelementptr [4 x i16], ptr %arr, i16 0, i16 0
  %p1 = getelementptr [4 x i16], ptr %arr, i16 0, i16 1
  %p2 = getelementptr [4 x i16], ptr %arr, i16 0, i16 2
  %p3 = getelementptr [4 x i16], ptr %arr, i16 0, i16 3
  store i16 10, ptr %p0
  store i16 20, ptr %p1
  store i16 30, ptr %p2
  store i16 40, ptr %p3
  %v0 = load i16, ptr %p0
  %v3 = load i16, ptr %p3
  %sum03 = add i16 %v0, %v3  ; 10 + 40 = 50
  %c0 = icmp eq i16 %sum03, 50
  br i1 %c0, label %set0, label %t1
set0:
  %s0 = load i16, ptr %status
  %s0a = or i16 %s0, 1
  store i16 %s0a, ptr %status
  br label %t1
t1:

  ; Bit 1: alloca array of 8 i8, store pattern and sum
  %bp0 = getelementptr [8 x i8], ptr %barr, i16 0, i16 0
  %bp7 = getelementptr [8 x i8], ptr %barr, i16 0, i16 7
  store i8 100, ptr %bp0
  store i8 55, ptr %bp7
  %bv0 = load i8, ptr %bp0
  %bv7 = load i8, ptr %bp7
  %bsum = add i8 %bv0, %bv7  ; 100 + 55 = 155
  %c1 = icmp eq i8 %bsum, -101  ; 155 as i8
  br i1 %c1, label %set1, label %t2
set1:
  %s1 = load i16, ptr %status
  %s1a = or i16 %s1, 2
  store i16 %s1a, ptr %status
  br label %t2
t2:

  ; Bit 2: overwrite and re-read (tests store doesn't alias wrong slot)
  store i16 999, ptr %p1
  %v1b = load i16, ptr %p1
  %v0b = load i16, ptr %p0    ; should still be 10
  %c2a = icmp eq i16 %v1b, 999
  %c2b = icmp eq i16 %v0b, 10
  %c2 = and i1 %c2a, %c2b
  br i1 %c2, label %set2, label %t3
set2:
  %s2 = load i16, ptr %status
  %s2a = or i16 %s2, 4
  store i16 %s2a, ptr %status
  br label %t3
t3:

  ; Bit 3: single i16 alloca (basic frame object)
  store i16 12345, ptr %single
  %sv = load i16, ptr %single
  %c3 = icmp eq i16 %sv, 12345
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
