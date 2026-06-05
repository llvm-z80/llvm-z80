; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O0 < %s -o /dev/null
; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s -o /dev/null
;
; Test that 16-bit EQ/NE comparisons work under register pressure.
; The XOR_CMP_EQ16/NE16 pseudo operands must be constrained to GR16
; so the register allocator can assign valid registers.

define i8 @cmp_eq_chain(i16 %a, i16 %b) {
  %sum = add i16 %a, %b
  %diff = sub i16 %a, %b
  %c1 = icmp eq i16 %sum, 100
  %c2 = icmp ne i16 %diff, 200
  %c3 = icmp eq i16 %a, %b
  %r1 = and i1 %c1, %c2
  %r2 = and i1 %r1, %c3
  %r = zext i1 %r2 to i8
  ret i8 %r
}

define i8 @cmp_eq_in_loop(i16 %target) {
entry:
  br label %loop
loop:
  %i = phi i16 [ 0, %entry ], [ %next, %loop ]
  %acc = phi i8 [ 0, %entry ], [ %newacc, %loop ]
  %cmp = icmp eq i16 %i, %target
  %bit = zext i1 %cmp to i8
  %newacc = or i8 %acc, %bit
  %next = add i16 %i, 1
  %done = icmp eq i16 %next, 10
  br i1 %done, label %exit, label %loop
exit:
  ret i8 %newacc
}
