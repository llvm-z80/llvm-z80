; RUN: llc -mtriple=z80 -O1 -verify-machineinstrs < %s -o /dev/null
; RUN: llc -mtriple=sm83 -O1 -verify-machineinstrs < %s -o /dev/null

; The legalizer scalarizes the wide vector away while #dbg_value still
; refers to it; the dangling debug operand must not reach RegBankSelect,
; whose 8/16-bit banks cannot carry the stale 256-bit type.
;
; In @g, selection folds the byval argument's frame address into the load
; and erases its definition, while #dbg_declare still refers to it. The
; debug operand must not be left on a register with no definition and no
; class, which the dead lane analysis would then mark undef.
;
; In @h, the source label becomes a debug instruction that has no value
; operands at all, which the cleanup must leave alone.

source_filename = "/tmp/dbgv.c"
target datalayout = "e-m:o-p:16:8-i16:8-i32:8-i64:8-i128:8-f32:8-f64:8-ve-n8:16"
target triple = "z80"

; Function Attrs: nofree norecurse nosync nounwind memory(none)
define dso_local i16 @f(i16 noundef %i, <2 x i128> noundef %v) local_unnamed_addr #0 !dbg !13 {
entry:
    #dbg_value(i16 %i, !24, !DIExpression(), !26)
    #dbg_value(<2 x i128> %v, !25, !DIExpression(), !26)
  br label %do.body, !dbg !27

do.body:                                          ; preds = %do.body, %entry
  %i.addr.0 = phi i16 [ %i, %entry ], [ %dec, %do.body ]
  %v.addr.0 = phi <2 x i128> [ %v, %entry ], [ %rem, %do.body ]
    #dbg_value(<2 x i128> %v.addr.0, !25, !DIExpression(), !26)
    #dbg_value(i16 %i.addr.0, !24, !DIExpression(), !26)
  %dec = add i16 %i.addr.0, -1, !dbg !28
    #dbg_value(i16 %dec, !24, !DIExpression(), !26)
  %not = xor <2 x i128> %v.addr.0, splat (i128 -1), !dbg !30
  %rem = urem <2 x i128> %v.addr.0, %not, !dbg !31
    #dbg_value(<2 x i128> %rem, !25, !DIExpression(), !26)
  %tobool.not = icmp eq i16 %dec, 0, !dbg !32
  br i1 %tobool.not, label %do.end, label %do.body, !dbg !33, !llvm.loop !34

do.end:                                           ; preds = %do.body
  %0 = bitcast <2 x i128> %rem to <16 x i16>, !dbg !38
  %conv = extractelement <16 x i16> %0, i64 0, !dbg !38
  ret i16 %conv, !dbg !39
}

%struct.S = type { i8, i8, i8, i8, i8, i8, i8, i8 }

define dso_local zeroext i8 @g(ptr noundef readonly byval(%struct.S) align 1 captures(none) %s) local_unnamed_addr !dbg !40 {
entry:
    #dbg_declare(ptr %s, !45, !DIExpression(), !46)
  %b = getelementptr inbounds nuw i8, ptr %s, i16 1, !dbg !47
  %0 = load i8, ptr %b, align 1, !dbg !47
  ret i8 %0, !dbg !47
}

define dso_local i16 @h(i16 noundef %x) local_unnamed_addr !dbg !50 {
entry:
  %z = icmp eq i16 %x, 0, !dbg !54
  br i1 %z, label %set, label %out, !dbg !54

set:
  br label %out, !dbg !54

out:
  %r = phi i16 [ 3, %set ], [ %x, %entry ]
    #dbg_label(!53, !54)
  ret i16 %r, !dbg !54
}

attributes #0 = { nofree norecurse nosync nounwind memory(none) "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3, !4, !5, !6}
!llvm.ident = !{!7}
!llvm.errno.tbaa = !{!8}

!0 = distinct !DICompileUnit(language: DW_LANG_C11, file: !1, producer: "clang version 24.0.0git (git@github.com:zlfn/llvm-z80.git 99ed25f17486cf6b246a0acaf4e8f667df3655ec)", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "/tmp/dbgv.c", directory: "/home/zlfn/Main/llvm-z80/build", checksumkind: CSK_MD5, checksum: "ed8ccfe2a41d9bf8929e10a871f7aae6")
!2 = !{i32 7, !"Dwarf Version", i32 5}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = !{i32 1, !"wchar_size", i32 2}
!5 = !{i32 7, !"frame-pointer", i32 2}
!6 = !{i32 7, !"debug-info-assignment-tracking", i1 true}
!7 = !{!"clang version 24.0.0git (git@github.com:zlfn/llvm-z80.git 99ed25f17486cf6b246a0acaf4e8f667df3655ec)"}
!8 = !{!9, !10, i64 0}
!9 = !{!"__libc_errno", !10, i64 0}
!10 = !{!"int", !11, i64 0}
!11 = !{!"omnipotent char", !12, i64 0}
!12 = !{!"Simple C/C++ TBAA"}
!13 = distinct !DISubprogram(name: "f", scope: !14, file: !14, line: 2, type: !15, scopeLine: 2, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !23, keyInstructions: true)
!14 = !DIFile(filename: "/tmp/dbgv.c", directory: "", checksumkind: CSK_MD5, checksum: "ed8ccfe2a41d9bf8929e10a871f7aae6")
!15 = !DISubroutineType(types: !16)
!16 = !{!17, !17, !18}
!17 = !DIBasicType(name: "unsigned int", size: 16, encoding: DW_ATE_unsigned)
!18 = !DIDerivedType(tag: DW_TAG_typedef, name: "v2ti", file: !14, line: 1, baseType: !19)
!19 = !DICompositeType(tag: DW_TAG_array_type, baseType: !20, size: 256, flags: DIFlagVector, elements: !21)
!20 = !DIBasicType(name: "unsigned __int128", size: 128, encoding: DW_ATE_unsigned)
!21 = !{!22}
!22 = !DISubrange(count: 2)
!23 = !{!24, !25}
!24 = !DILocalVariable(name: "i", arg: 1, scope: !13, file: !14, line: 2, type: !17)
!25 = !DILocalVariable(name: "v", arg: 2, scope: !13, file: !14, line: 2, type: !18)
!26 = !DILocation(line: 0, scope: !13)
!27 = !DILocation(line: 3, column: 3, scope: !13)
!28 = !DILocation(line: 3, column: 9, scope: !29, atomGroup: 1, atomRank: 2)
!29 = distinct !DILexicalBlock(scope: !13, file: !14, line: 3, column: 6)
!30 = !DILocation(line: 3, column: 18, scope: !29)
!31 = !DILocation(line: 3, column: 15, scope: !29, atomGroup: 2, atomRank: 2)
!32 = !DILocation(line: 3, column: 22, scope: !29, atomGroup: 3, atomRank: 1)
!33 = !DILocation(line: 3, column: 22, scope: !29, atomGroup: 4, atomRank: 1)
!34 = distinct !{!34, !27, !35, !36, !37}
!35 = !DILocation(line: 3, column: 32, scope: !13)
!36 = !{!"llvm.loop.mustprogress"}
!37 = !{!"llvm.loop.unroll.disable"}
!38 = !DILocation(line: 4, column: 10, scope: !13, atomGroup: 5, atomRank: 2)
!39 = !DILocation(line: 4, column: 3, scope: !13, atomGroup: 5, atomRank: 1)
!40 = distinct !DISubprogram(name: "g", scope: !14, file: !14, line: 6, type: !41, scopeLine: 6, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !44)
!41 = !DISubroutineType(types: !42)
!42 = !{!43, !48}
!43 = !DIBasicType(name: "unsigned char", size: 8, encoding: DW_ATE_unsigned_char)
!44 = !{!45}
!45 = !DILocalVariable(name: "s", arg: 1, scope: !40, file: !14, line: 6, type: !48)
!46 = !DILocation(line: 0, scope: !40)
!47 = !DILocation(line: 6, column: 30, scope: !40)
!48 = !DICompositeType(tag: DW_TAG_structure_type, name: "S", file: !14, line: 5, size: 64, elements: !49)
!49 = !{}
!50 = distinct !DISubprogram(name: "h", scope: !14, file: !14, line: 8, type: !51, scopeLine: 8, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!51 = !DISubroutineType(types: !52)
!52 = !{!17, !17}
!53 = !DILabel(scope: !50, name: "out", file: !14, line: 10)
!54 = !DILocation(line: 10, scope: !50)
