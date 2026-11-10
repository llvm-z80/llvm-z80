; RUN: llc -mtriple=z80 -O2 -verify-machineinstrs < %s | FileCheck %s --check-prefix=Z80
; RUN: opt -strip-debug < %s | llc -mtriple=z80 -O2 | FileCheck %s --check-prefix=Z80
; RUN: llc -mtriple=sm83 -O2 -verify-machineinstrs < %s | FileCheck %s --check-prefix=SM83
; RUN: opt -strip-debug < %s | llc -mtriple=sm83 -O2 | FileCheck %s --check-prefix=SM83
;
; Spill slots accessed in stack order within one block become PUSH and POP.
; The function is left without a frame. Frame accesses under a push are
; shifted by the bytes pushed. Debug information does not change the code.

declare void @ext()

; The loop from issue #133. The system call clobbers every register.
define void @asm_loop(ptr %p, i16 %n) {
; Z80-LABEL:  _asm_loop:
; Z80-NOT:    push af
; Z80:        push hl
; Z80-NEXT:   push de
; Z80-NEXT:   ld e,(hl)
; Z80-NEXT:   ld c,2
; Z80-NEXT:   ;APP
; Z80-NEXT:   call 5
; Z80-NEXT:   ;NO_APP
; Z80-NEXT:   pop de
; Z80-NEXT:   pop hl
; Z80-NEXT:   inc hl
; Z80-NEXT:   dec de
; Z80:        ret
; SM83-LABEL: _asm_loop:
; SM83-NOT:   add sp
; SM83:       push de
; SM83-NEXT:  push bc
; SM83:       call 5
; SM83-NEXT:  ;NO_APP
; SM83-NEXT:  pop bc
; SM83-NEXT:  pop de
entry:
  %z = icmp eq i16 %n, 0
  br i1 %z, label %done, label %loop

loop:
  %ptr = phi ptr [ %p, %entry ], [ %pn, %loop ]
  %cnt = phi i16 [ %n, %entry ], [ %cn, %loop ]
  %c = load i8, ptr %ptr
  %r = call { i8, i8, i16, i8, i8, i8 } asm sideeffect "call 5", "=&{b},=&{d},=&{hl},=&{a},={c},={e},{c},{e},~{flags}"(i8 2, i8 %c)
  %pn = getelementptr i8, ptr %ptr, i16 1
  %cn = add i16 %cnt, -1
  %z2 = icmp eq i16 %cn, 0
  br i1 %z2, label %done, label %loop

done:
  ret void
}

; The reload of %b pops into a pair other than the pushed pair.
define i16 @across_call(i16 %a, i16 %b) {
; Z80-LABEL:  _across_call:
; Z80:        push de
; Z80-NEXT:   push hl
; Z80-NEXT:   call _ext
; Z80-NEXT:   pop hl
; Z80-NEXT:   pop bc
; Z80-NEXT:   add hl,bc
; SM83-LABEL: _across_call:
; SM83:       push bc
; SM83-NEXT:  push de
; SM83-NEXT:  call _ext
; SM83-NEXT:  pop hl
; SM83-NEXT:  pop bc
; SM83-NEXT:  add hl,bc
  call void @ext()
  %s = add i16 %a, %b
  ret i16 %s
}

; The reloads after the call are inside the call sequence.
declare void @put(i8)

define void @call_loop(ptr %p, i16 %n) {
; Z80-LABEL:  _call_loop:
; Z80:        push de
; Z80-NEXT:   ld a,(hl)
; Z80-NEXT:   inc hl
; Z80-NEXT:   push hl
; Z80-NEXT:   call _put
; Z80-NEXT:   pop hl
; Z80-NEXT:   pop de
; SM83-LABEL: _call_loop:
; SM83:       push bc
; SM83-NEXT:  ld a,(de)
; SM83-NEXT:  inc de
; SM83-NEXT:  push de
; SM83-NEXT:  call _put
; SM83-NEXT:  pop de
; SM83-NEXT:  pop bc
entry:
  %z = icmp eq i16 %n, 0
  br i1 %z, label %done, label %loop

loop:
  %pp = phi ptr [ %p, %entry ], [ %pn, %loop ]
  %nn = phi i16 [ %n, %entry ], [ %nd, %loop ]
  %nd = add i16 %nn, -1
  %c = load i8, ptr %pp
  %pn = getelementptr i8, ptr %pp, i16 1
  call void @put(i8 %c)
  %e = icmp eq i16 %nd, 0
  br i1 %e, label %done, label %loop

done:
  ret void
}

; %pos is a stack argument read while a spill is pushed.
define i16 @stack_args(i16 %val, i16 %bits, i8 %pos, i8 %n) {
; Z80-LABEL:  _stack_args:
; Z80:        push de
; Z80-NEXT:   ld hl,4
; Z80-NEXT:   add hl,sp
; Z80-NEXT:   ld c,(hl)
; SM83-LABEL: _stack_args:
; SM83:       push bc
; SM83:       push hl
; SM83-NEXT:  ldhl sp,6
; SM83-NEXT:  ld b,(hl)
; SM83-NEXT:  pop hl
  %m0 = shl i16 -1, %bits
  %mask = xor i16 %m0, -1
  %sh = zext i8 %pos to i16
  %m1 = shl i16 %mask, %sh
  %nm = xor i16 %m1, -1
  %cl = and i16 %val, %nm
  %b = and i16 %bits, %mask
  %nn = zext i8 %n to i16
  %bs = shl i16 %b, %nn
  %r = or i16 %cl, %bs
  ret i16 %r
}

; A debug value names the spill slot of %k.
define i16 @debug_value(i16 %k, ptr %p, ptr %q) optsize !dbg !3 {
; Z80-LABEL:  _debug_value:
; Z80:        push de
; Z80-NEXT:   ld hl,4
; Z80-NEXT:   add hl,sp
; SM83-LABEL: _debug_value:
; SM83:       push bc
; SM83-NEXT:  push de
; SM83-NEXT:  ldhl sp,6
    #dbg_value(i16 %k, !7, !DIExpression(), !9)
  store i16 0, ptr %q, align 1
  %v = shl i16 %k, 1
  store i16 %v, ptr %p, align 1
  ret i16 %k
}

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2}

!0 = distinct !DICompileUnit(language: DW_LANG_C11, file: !1, isOptimized: true, emissionKind: FullDebug)
!1 = !DIFile(filename: "t.c", directory: "/")
!2 = !{i32 2, !"Debug Info Version", i32 3}
!3 = distinct !DISubprogram(name: "debug_value", scope: !1, file: !1, line: 1, type: !4, scopeLine: 1, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !6)
!4 = !DISubroutineType(types: !5)
!5 = !{null}
!6 = !{}
!7 = !DILocalVariable(name: "k", arg: 1, scope: !3, file: !1, line: 1, type: !8)
!8 = !DIBasicType(name: "int", size: 16, encoding: DW_ATE_signed)
!9 = !DILocation(line: 0, scope: !3)
