; RUN: llc -mtriple=z80 -O2 < %s | FileCheck %s --check-prefix=Z80
; RUN: opt -strip-debug < %s | llc -mtriple=z80 -O2 | FileCheck %s --check-prefix=Z80
; RUN: llc -mtriple=sm83 -O2 < %s | FileCheck %s --check-prefix=SM83
; RUN: opt -strip-debug < %s | llc -mtriple=sm83 -O2 | FileCheck %s --check-prefix=SM83
;
; Debug information does not change the code, so each function is checked
; with it and without it. In @sister a debug value sits between a reload and
; the instruction the pre-emit peephole folds the reload into. In @bar a
; debug value names a frame slot, which is no access to count when an
; optsize build decides whether its wide slots stay on the stack.

target datalayout = "e-m:o-p:16:8-i16:8-i32:8-i64:8-i128:8-f16:8-f32:8-f64:8-f128:8-ve-a:8-n8:16"

%struct.foo = type { i16, i16, i16 }

define void @sister(ptr byval(%struct.foo) %f, i16 %b) "frame-pointer"="all" !dbg !3 {
; Z80-LABEL: _sister:
; Z80:       ld a,(ix+10)
; Z80-NEXT:  or (ix+11)
entry:
    #dbg_value(i16 %b, !8, !DIExpression(), !10)
  %cmp.not = icmp eq i16 0, %b
  br i1 %cmp.not, label %if.then.i, label %brother.exit

if.then.i:
  tail call void @abort()
  ret void

brother.exit:
  ret void
}

define i16 @bar(i16 %k, ptr %foo, ptr %oldfoo) optsize !dbg !11 {
; SM83-LABEL: _bar:
; SM83:       ld hl,L_bar.frame+2
entry:
    #dbg_value(i16 %k, !13, !DIExpression(), !15)
  store i16 0, ptr %oldfoo, align 1
  %bf.value = shl i16 %k, 1
  store i16 %bf.value, ptr %foo, align 1
  ret i16 %k
}

declare void @abort()

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2}

!0 = distinct !DICompileUnit(language: DW_LANG_C11, file: !1, isOptimized: true, emissionKind: FullDebug)
!1 = !DIFile(filename: "t.c", directory: "/")
!2 = !{i32 2, !"Debug Info Version", i32 3}
!3 = distinct !DISubprogram(name: "sister", scope: !1, file: !1, line: 14, type: !5, scopeLine: 15, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !7)
!5 = distinct !DISubroutineType(types: !6)
!6 = !{null}
!7 = !{}
!8 = !DILocalVariable(name: "b", arg: 2, scope: !3, file: !1, line: 14, type: !9)
!9 = !DIBasicType(name: "int", size: 16, encoding: DW_ATE_signed)
!10 = !DILocation(line: 0, scope: !3)
!11 = distinct !DISubprogram(name: "bar", scope: !1, file: !1, line: 11, type: !5, scopeLine: 12, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !7)
!13 = !DILocalVariable(name: "k", arg: 1, scope: !11, file: !1, line: 11, type: !14)
!14 = !DIBasicType(name: "unsigned int", size: 16, encoding: DW_ATE_unsigned)
!15 = !DILocation(line: 0, scope: !11)
