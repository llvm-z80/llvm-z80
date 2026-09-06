; RUN: llc -mtriple=z80 -O1 -verify-machineinstrs < %s -o /dev/null
; RUN: llc -mtriple=z80 -O0 -verify-machineinstrs < %s -o /dev/null

; A stack save whose only use is the restore produces a bare SP copy that
; nothing else constrains; the register bank must cover the class the
; selector assigns to it (HL and the index registers).

declare void @use(ptr)

define void @vla_loop(i16 %n) {
entry:
  br label %loop
loop:
  %i = phi i16 [ 0, %entry ], [ %inc, %loop ]
  %save = call ptr @llvm.stacksave()
  %arr = alloca i16, i16 %n, align 1
  call void @use(ptr %arr)
  call void @llvm.stackrestore(ptr %save)
  %inc = add i16 %i, 1
  %c = icmp ult i16 %inc, 4
  br i1 %c, label %loop, label %exit
exit:
  ret void
}
