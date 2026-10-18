; RUN: %llc %s -o %t.s
; RUN: %cc %t.s -o %t
; RUN: %run --expect-result=0x0003 %t

; Relaxation of a conditional JR with a target out of JR range

@v1 = global i8 0
@v2 = global i8 0

define i16 @main() {
entry:
  %status = alloca i16
  store i16 0, ptr %status

  ; Bit 0: the else path (x = 5) returns 0
  %r1 = call i16 @big_branch(i8 5)
  %c1 = icmp eq i16 %r1, 0
  br i1 %c1, label %set1, label %test2
set1:
  %s1 = load i16, ptr %status
  %s1a = or i16 %s1, 1
  store i16 %s1a, ptr %status
  br label %test2

test2:
  ; Bit 1: the then path (x = 20) returns 1
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

; The cold then block lands after the large else block, so the branch from
; entry to then is out of JR range.
define i16 @big_branch(i8 %x) {
entry:
  %cmp = icmp ugt i8 %x, 10
  br i1 %cmp, label %then, label %else, !prof !0

then:
  ; Cold path
  store volatile i8 99, ptr @v1
  br label %end

else:
  ; Hot path of more than 127 bytes
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
