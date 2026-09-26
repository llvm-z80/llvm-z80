; RUN: llc -mtriple=z80 -O1 -verify-machineinstrs < %s -o /dev/null
; RUN: llc -mtriple=sm83 -O1 -verify-machineinstrs < %s -o /dev/null

; Z80DanglingDebugCleanup must not call MachineInstr::debug_operands() on a
; debug instruction that is not "debug-value-like" (DBG_LABEL / DBG_PHI).
; debug_operands() asserts isDebugValueLike() and, for a DBG_LABEL, computes
; operands().drop_front(2) over a 1-operand instruction -- an out-of-bounds
; range. In a release (no-assert) build the pass then iterates garbage and
; segfaults in MachineOperand::setReg. This crashed the rcbios clang build
; (@bios_write_c carries a DBG_LABEL for a `goto` target). The pass now gates
; on isDebugValueLike() so DBG_LABEL/DBG_PHI are skipped. See ravn/llvm-z80#312.

source_filename = "/tmp/lbl.c"
target datalayout = "e-m:o-p:16:8-i16:8-i32:8-i64:8-i128:8-f32:8-f64:8-ve-n8:16"
target triple = "z80"

; A `goto` label under -g emits a #dbg_label -> DBG_LABEL in MIR.
define dso_local i16 @f(i16 noundef %0) local_unnamed_addr !dbg !12 {
    #dbg_value(i16 %0, !18, !DIExpression(), !20)
  %2 = tail call i16 @llvm.umax.i16(i16 %0, i16 1), !dbg !21
    #dbg_value(i16 %2, !18, !DIExpression(), !20)
    #dbg_label(!19, !23)
  ret i16 %2, !dbg !24
}

declare i16 @llvm.umax.i16(i16, i16)

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3, !4, !5}

!0 = distinct !DICompileUnit(language: DW_LANG_C11, file: !1, producer: "clang", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "lbl.c", directory: "")
!2 = !{i32 7, !"Dwarf Version", i32 5}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = !{i32 1, !"wchar_size", i32 2}
!5 = !{i32 7, !"debug-info-assignment-tracking", i1 true}
!12 = distinct !DISubprogram(name: "f", scope: !13, file: !13, line: 2, type: !14, scopeLine: 2, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !17)
!13 = !DIFile(filename: "lbl.c", directory: "")
!14 = !DISubroutineType(types: !15)
!15 = !{!16, !16}
!16 = !DIBasicType(name: "int", size: 16, encoding: DW_ATE_signed)
!17 = !{!18, !19}
!18 = !DILocalVariable(name: "n", arg: 1, scope: !12, file: !13, line: 2, type: !16)
!19 = !DILabel(scope: !12, name: "done", file: !13, line: 5, column: 1)
!20 = !DILocation(line: 0, scope: !12)
!21 = !DILocation(line: 3, column: 7, scope: !22)
!22 = distinct !DILexicalBlock(scope: !12, file: !13, line: 3, column: 7)
!23 = !DILocation(line: 5, column: 1, scope: !12)
!24 = !DILocation(line: 6, column: 3, scope: !12)
