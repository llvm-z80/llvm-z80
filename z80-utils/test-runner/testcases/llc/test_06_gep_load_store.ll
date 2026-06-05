; Test 06: GEP, load, store patterns with various offsets
; Tests G_GEP, G_LOAD, G_STORE with array/struct addressing
; expect 0x000F

define void @_start() {
  call void asm sideeffect "ld sp, #0xFFFE", ""()
  %r = call i16 @main()
  call void asm sideeffect ".globl _halt\0A_halt:\0Ahalt", ""()
  ret void
}

define i16 @main() {
  %status = alloca i16
  %arr = alloca [4 x i8]
  %arr16 = alloca [3 x i16]
  %val32 = alloca i32
  %pair = alloca [2 x i16]
  store i16 0, ptr %status

  ; Bit 0: store/load i8 array with GEP
  %p0 = getelementptr [4 x i8], ptr %arr, i16 0, i16 0
  store i8 10, ptr %p0
  %p1 = getelementptr [4 x i8], ptr %arr, i16 0, i16 1
  store i8 20, ptr %p1
  %p2 = getelementptr [4 x i8], ptr %arr, i16 0, i16 2
  store i8 30, ptr %p2
  %p3 = getelementptr [4 x i8], ptr %arr, i16 0, i16 3
  store i8 40, ptr %p3
  %v0 = load i8, ptr %p0
  %v1 = load i8, ptr %p1
  %v2 = load i8, ptr %p2
  %v3 = load i8, ptr %p3
  %sum01 = add i8 %v0, %v1
  %sum012 = add i8 %sum01, %v2
  %sum0123 = add i8 %sum012, %v3
  %c0 = icmp eq i8 %sum0123, 100
  br i1 %c0, label %set0, label %t1
set0:
  %s0 = load i16, ptr %status
  store i16 1, ptr %status
  br label %t1
t1:

  ; Bit 1: store/load i16 array
  %q0 = getelementptr [3 x i16], ptr %arr16, i16 0, i16 0
  store i16 1000, ptr %q0
  %q1 = getelementptr [3 x i16], ptr %arr16, i16 0, i16 1
  store i16 2000, ptr %q1
  %q2 = getelementptr [3 x i16], ptr %arr16, i16 0, i16 2
  store i16 3000, ptr %q2
  %w0 = load i16, ptr %q0
  %w1 = load i16, ptr %q1
  %w2 = load i16, ptr %q2
  %wsum = add i16 %w0, %w1
  %wsum2 = add i16 %wsum, %w2
  %c1 = icmp eq i16 %wsum2, 6000
  br i1 %c1, label %set1, label %t2
set1:
  %s1 = load i16, ptr %status
  %s1a = or i16 %s1, 2
  store i16 %s1a, ptr %status
  br label %t2
t2:

  ; Bit 2: store/load i32 (multi-word at stack offset)
  store i32 305419896, ptr %val32  ; 0x12345678
  %r32 = load i32, ptr %val32
  %lo16 = trunc i32 %r32 to i16
  %c2 = icmp eq i16 %lo16, 22136  ; 0x5678
  br i1 %c2, label %set2, label %t3
set2:
  %s2 = load i16, ptr %status
  %s2a = or i16 %s2, 4
  store i16 %s2a, ptr %status
  br label %t3
t3:

  ; Bit 3: write via pointer then read back
  %pa = getelementptr [2 x i16], ptr %pair, i16 0, i16 0
  %pb = getelementptr [2 x i16], ptr %pair, i16 0, i16 1
  store i16 111, ptr %pa
  store i16 222, ptr %pb
  %ra = load i16, ptr %pa
  %rb = load i16, ptr %pb
  %rsum = add i16 %ra, %rb
  %c3 = icmp eq i16 %rsum, 333
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
