; Test 02: phi nodes with multiple predecessors
; Tests G_PHI lowering — C generates phi but rarely with 3+ predecessors
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

  ; Bit 0: simple two-predecessor phi
  %c0 = icmp ugt i16 10, 5
  br i1 %c0, label %then0, label %else0
then0:
  br label %merge0
else0:
  br label %merge0
merge0:
  %p0 = phi i16 [42, %then0], [99, %else0]
  %t0 = icmp eq i16 %p0, 42
  br i1 %t0, label %set0, label %test1
set0:
  %s0 = load i16, ptr %status
  %s0a = or i16 %s0, 1
  store i16 %s0a, ptr %status
  br label %test1
test1:

  ; Bit 1: phi in a loop (accumulator pattern)
  br label %loop1
loop1:
  %i1 = phi i16 [0, %test1], [%i1n, %loop1]
  %acc1 = phi i16 [0, %test1], [%acc1n, %loop1]
  %acc1n = add i16 %acc1, %i1
  %i1n = add i16 %i1, 1
  %done1 = icmp eq i16 %i1n, 10
  br i1 %done1, label %check1, label %loop1
check1:
  ; sum(0..9) = 45
  %t1 = icmp eq i16 %acc1n, 45
  br i1 %t1, label %set1, label %test2
set1:
  %s1 = load i16, ptr %status
  %s1a = or i16 %s1, 2
  store i16 %s1a, ptr %status
  br label %test2
test2:

  ; Bit 2: three-predecessor phi
  %val = add i16 7, 0
  switch i16 %val, label %sw_def [
    i16 5, label %sw5
    i16 7, label %sw7
  ]
sw5:
  br label %sw_merge
sw7:
  br label %sw_merge
sw_def:
  br label %sw_merge
sw_merge:
  %p2 = phi i16 [100, %sw5], [200, %sw7], [300, %sw_def]
  %t2 = icmp eq i16 %p2, 200
  br i1 %t2, label %set2, label %test3
set2:
  %s2 = load i16, ptr %status
  %s2a = or i16 %s2, 4
  store i16 %s2a, ptr %status
  br label %test3
test3:

  ; Bit 3: nested phi (if-else inside loop)
  br label %loop3
loop3:
  %i3 = phi i16 [0, %test3], [%i3n, %loop3_end]
  %sum3 = phi i16 [0, %test3], [%sum3n, %loop3_end]
  %odd3 = and i16 %i3, 1
  %is_odd = icmp ne i16 %odd3, 0
  br i1 %is_odd, label %add_it, label %skip_it
add_it:
  %added = add i16 %sum3, %i3
  br label %loop3_end
skip_it:
  br label %loop3_end
loop3_end:
  %sum3n = phi i16 [%added, %add_it], [%sum3, %skip_it]
  %i3n = add i16 %i3, 1
  %done3 = icmp eq i16 %i3n, 10
  br i1 %done3, label %check3, label %loop3
check3:
  ; sum of odd numbers 1+3+5+7+9 = 25
  %t3 = icmp eq i16 %sum3n, 25
  br i1 %t3, label %set3, label %done
set3:
  %s3 = load i16, ptr %status
  %s3a = or i16 %s3, 8
  store i16 %s3a, ptr %status
  br label %done
done:
  %ret = load i16, ptr %status
  ret i16 %ret
}
