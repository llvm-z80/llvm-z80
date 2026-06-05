; Test 20: JR branch relaxation — JR overflow triggers condition reversal + JP trampoline
; When block placement moves a cold block far from entry, the conditional JR
; must be relaxed (reversed condition JR + unconditional JP).
; expect 0x0003

@v1 = global i8 0
@v2 = global i8 0

define void @_start() {
  call void asm sideeffect "ld sp, #0xFFFE", ""()
  %r = call i16 @main()
  call void asm sideeffect ".globl _halt\0A_halt:\0Ahalt", ""()
  ret void
}

define i16 @main() {
entry:
  %status = alloca i16
  store i16 0, ptr %status

  ; Test 1: take the else path (x=5, not > 10) — should return 0
  %r1 = call i16 @big_branch(i8 5)
  %c1 = icmp eq i16 %r1, 0
  br i1 %c1, label %set1, label %test2
set1:
  %s1 = load i16, ptr %status
  %s1a = or i16 %s1, 1
  store i16 %s1a, ptr %status
  br label %test2

test2:
  ; Test 2: take the then path (x=20, > 10) — should return 1
  %r2 = call i16 @big_branch(i8 20)
  %c2 = icmp eq i16 %r2, 1
  br i1 %c2, label %set2, label %done
set2:
  %s2 = load i16, ptr %status
  %s2a = or i16 %s2, 2
  store i16 %s2a, ptr %status
  br label %done

done:
  %result = load i16, ptr %status
  ret i16 %result
}

; Function with a conditional branch that overflows JR range.
; The "then" block is cold (weight 1), so block placement moves it after
; the large "else" block. The conditional JR from entry to "then" must be
; relaxed into a reversed-condition JR + JP trampoline.
define i16 @big_branch(i8 %x) {
entry:
  %cmp = icmp ugt i8 %x, 10
  br i1 %cmp, label %then, label %else, !prof !0

then:
  ; Cold path — placed at end by block placement
  store volatile i8 99, ptr @v1
  br label %end

else:
  ; Hot path — large block (>127 bytes) placed as fallthrough from entry
  store volatile i8 1, ptr @v1
  store volatile i8 2, ptr @v2
  store volatile i8 3, ptr @v1
  store volatile i8 4, ptr @v2
  store volatile i8 5, ptr @v1
  store volatile i8 6, ptr @v2
  store volatile i8 7, ptr @v1
  store volatile i8 8, ptr @v2
  store volatile i8 9, ptr @v1
  store volatile i8 10, ptr @v2
  store volatile i8 11, ptr @v1
  store volatile i8 12, ptr @v2
  store volatile i8 13, ptr @v1
  store volatile i8 14, ptr @v2
  store volatile i8 15, ptr @v1
  store volatile i8 16, ptr @v2
  store volatile i8 17, ptr @v1
  store volatile i8 18, ptr @v2
  store volatile i8 19, ptr @v1
  store volatile i8 20, ptr @v2
  store volatile i8 21, ptr @v1
  store volatile i8 22, ptr @v2
  store volatile i8 23, ptr @v1
  store volatile i8 24, ptr @v2
  store volatile i8 25, ptr @v1
  store volatile i8 26, ptr @v2
  store volatile i8 27, ptr @v1
  store volatile i8 28, ptr @v2
  store volatile i8 29, ptr @v1
  store volatile i8 30, ptr @v2
  br label %end

end:
  %result = phi i16 [ 1, %then ], [ 0, %else ]
  ret i16 %result
}

!0 = !{!"branch_weights", i32 1, i32 1000}
