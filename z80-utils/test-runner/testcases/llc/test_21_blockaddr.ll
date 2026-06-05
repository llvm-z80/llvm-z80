; Test 21: G_BLOCK_ADDR (computed goto / GCC &&label extension)
; Tests blockaddress materialization and indirectbr
; expect 0x000F

define void @_start() {
  call void asm sideeffect "ld sp, #0xFFFE", ""()
  %r = call i16 @main()
  call void asm sideeffect ".globl _halt\0A_halt:\0Ahalt", ""()
  ret void
}

; dispatch via computed goto: selects operation based on index
define i16 @do_op(i16 %idx, i16 %a, i16 %b) {
entry:
  %table = alloca [3 x ptr]
  store ptr blockaddress(@do_op, %op_add), ptr %table
  %p1 = getelementptr ptr, ptr %table, i32 1
  store ptr blockaddress(@do_op, %op_sub), ptr %p1
  %p2 = getelementptr ptr, ptr %table, i32 2
  store ptr blockaddress(@do_op, %op_xor), ptr %p2

  %gep = getelementptr ptr, ptr %table, i16 %idx
  %target = load ptr, ptr %gep
  indirectbr ptr %target, [label %op_add, label %op_sub, label %op_xor]

op_add:
  %r1 = add i16 %a, %b
  ret i16 %r1

op_sub:
  %r2 = sub i16 %a, %b
  ret i16 %r2

op_xor:
  %r3 = xor i16 %a, %b
  ret i16 %r3
}

define i16 @main() {
entry:
  ; do_op(0, 50, 50) = 50 + 50 = 100
  %r1 = call i16 @do_op(i16 0, i16 50, i16 50)
  ; do_op(1, 100, 85) = 100 - 85 = 15
  %r2 = call i16 @do_op(i16 1, i16 %r1, i16 85)
  ; do_op(2, 15, 0) = 15 ^ 0 = 15
  %r3 = call i16 @do_op(i16 2, i16 %r2, i16 0)
  ret i16 %r3
}
