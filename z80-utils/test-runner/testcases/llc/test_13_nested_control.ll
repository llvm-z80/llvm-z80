; Test 13: nested control flow (if-else chains, nested loops)
; Tests complex CFG patterns
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

  ; Bit 0: nested if-else (triangle)
  %x = add i16 0, 15
  %cond1 = icmp ugt i16 %x, 10
  br i1 %cond1, label %outer_true, label %outer_false

outer_true:
  %cond2 = icmp ult i16 %x, 20
  br i1 %cond2, label %inner_true, label %outer_false

inner_true:
  ; x > 10 && x < 20 → set bit 0
  %s0 = load i16, ptr %status
  %s0a = or i16 %s0, 1
  store i16 %s0a, ptr %status
  br label %outer_false

outer_false:
  ; Bit 1: simple counted loop (sum 1..5 = 15)
  br label %loop1_head

loop1_head:
  %i1 = phi i16 [1, %outer_false], [%i1next, %loop1_body]
  %sum1 = phi i16 [0, %outer_false], [%sum1next, %loop1_body]
  %loop1_cond = icmp ule i16 %i1, 5
  br i1 %loop1_cond, label %loop1_body, label %loop1_done

loop1_body:
  %sum1next = add i16 %sum1, %i1
  %i1next = add i16 %i1, 1
  br label %loop1_head

loop1_done:
  %c1 = icmp eq i16 %sum1, 15
  br i1 %c1, label %set1, label %t2

set1:
  %s1 = load i16, ptr %status
  %s1a = or i16 %s1, 2
  store i16 %s1a, ptr %status
  br label %t2

t2:
  ; Bit 2: loop with early exit (find first >3 in sequence 1,2,3,4,5)
  br label %loop2_head

loop2_head:
  %i2 = phi i16 [1, %t2], [%i2next, %loop2_cont]
  %i2cond = icmp ugt i16 %i2, 5
  br i1 %i2cond, label %loop2_notfound, label %loop2_check

loop2_check:
  %found = icmp ugt i16 %i2, 3
  br i1 %found, label %loop2_found, label %loop2_cont

loop2_cont:
  %i2next = add i16 %i2, 1
  br label %loop2_head

loop2_found:
  %c2 = icmp eq i16 %i2, 4  ; first > 3 is 4
  br i1 %c2, label %set2, label %t3

loop2_notfound:
  br label %t3

set2:
  %s2 = load i16, ptr %status
  %s2a = or i16 %s2, 4
  store i16 %s2a, ptr %status
  br label %t3

t3:
  ; Bit 3: do-while style (at least one iteration)
  %init = add i16 0, 100
  br label %loop3_body

loop3_body:
  %val3 = phi i16 [%init, %t3], [%val3next, %loop3_body]
  %val3next = sub i16 %val3, 10
  %loop3_cond = icmp ugt i16 %val3next, 0
  br i1 %loop3_cond, label %loop3_body, label %loop3_done

loop3_done:
  ; 100 - 10*10 = 0
  %c3 = icmp eq i16 %val3next, 0
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
