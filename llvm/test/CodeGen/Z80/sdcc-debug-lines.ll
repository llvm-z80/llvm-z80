; RUN: llc -mtriple=sm83-unknown-none-sdcc -z80-asm-format=sdasz80 < %s | FileCheck %s

; The sdasz80 listing marks code with the debug symbols of SDCC, and sdld
; copies them into the CDB file that a debugger reads.
;
;   C$<file>$<line>$<level>$<block>  The code of a source line starts here.
;   G$<name>$0$0                     A function starts here.
;   F<module>$<name>$0$0             A function with local linkage starts here.
;
; An instruction gets a line symbol when its line differs from the line of
; the instruction before it. A comment under the symbol names the path and
; the column, and the call sites of inlined code after `@[`, as the comment
; of a `.loc` does in the ELF listing.

; CHECK-LABEL: _add:
; CHECK:       G$add$0$0 = .
; CHECK-NEXT:  .globl G$add$0$0
; CHECK:       C$math.zig$3$0_0$0 = .
; CHECK-NEXT:  .globl C$math.zig$3$0_0$0
; CHECK-NEXT:  ; src/math.zig:3:17
; CHECK-NEXT:  add a,e
; CHECK-NOT:   C$
; CHECK:       xor #1
; CHECK-NOT:   C$
; CHECK:       add a,e

; Inlined code gets the symbol of its own line, and the line that calls it
; gets a symbol at the same address.

; CHECK-NEXT:  C$math.zig$4$0_0$0 = .
; CHECK-NEXT:  .globl C$math.zig$4$0_0$0
; CHECK-NEXT:  C$twice.zig$2$0_0$0 = .
; CHECK-NEXT:  .globl C$twice.zig$2$0_0$0
; CHECK-NEXT:  ; src/twice.zig:2:14 @[ src/math.zig:4:12 ]
; CHECK-NEXT:  add a,a
; CHECK-NEXT:  C$math.zig$5$0_0$0 = .
; CHECK-NEXT:  .globl C$math.zig$5$0_0$0
; CHECK-NEXT:  ; src/math.zig:5:5
; CHECK-NEXT:  ret

; A function starts without a line, so its first line gets a symbol even
; when the function before it ended on the same line. The block field
; counts the symbols of a line, so the second symbol of line 5 ends in 1.

; CHECK-LABEL: _same:
; CHECK:       C$math.zig$5$0_0$1 = .
; CHECK-NEXT:  .globl C$math.zig$5$0_0$1
; CHECK-NEXT:  ; src/math.zig:5:5
; CHECK-NEXT:  ret

; Code in front of the first line of a function belongs to the line that
; opens the function, as in the line table of DWARF.

; CHECK-LABEL: _opening:
; CHECK:       C$math.zig$8$0_0$0 = .
; CHECK-NEXT:  .globl C$math.zig$8$0_0$0
; CHECK-NEXT:  ; src/math.zig:8
; CHECK-NEXT:  inc a
; CHECK-NEXT:  C$math.zig$9$0_0$0 = .
; CHECK-NEXT:  .globl C$math.zig$9$0_0$0
; CHECK-NEXT:  ; src/math.zig:9:5
; CHECK-NEXT:  ret

; Each inlined call starts a line, even when two calls run the same line of
; the inlined function.

; CHECK-LABEL: _two_calls:
; CHECK:       C$math.zig$12$0_0$0 = .
; CHECK-NEXT:  .globl C$math.zig$12$0_0$0
; CHECK-NEXT:  C$twice.zig$2$0_0$1 = .
; CHECK-NEXT:  .globl C$twice.zig$2$0_0$1
; CHECK-NEXT:  ; src/twice.zig:2:14 @[ src/math.zig:12:9 ]
; CHECK-NEXT:  ld (bc),a
; CHECK-NEXT:  C$math.zig$13$0_0$0 = .
; CHECK-NEXT:  .globl C$math.zig$13$0_0$0
; CHECK-NEXT:  C$twice.zig$2$0_0$2 = .
; CHECK-NEXT:  .globl C$twice.zig$2$0_0$2
; CHECK-NEXT:  ; src/twice.zig:2:14 @[ src/math.zig:13:9 ]
; CHECK-NEXT:  ld a,e

; A call site gets one symbol. Line 17 has code of its own in front of the
; inlined call, so the call adds no symbol for line 17, and neither does
; the second line of the inlined function. The code of line 17 after the
; call starts line 17 again.

; CHECK-LABEL: _around_call:
; CHECK:       C$math.zig$17$0_0$0 = .
; CHECK-NEXT:  .globl C$math.zig$17$0_0$0
; CHECK-NEXT:  ; src/math.zig:17:5
; CHECK-NEXT:  ld (hl),a
; CHECK-NEXT:  C$twice.zig$2$0_0$3 = .
; CHECK-NEXT:  .globl C$twice.zig$2$0_0$3
; CHECK-NEXT:  ; src/twice.zig:2:14 @[ src/math.zig:17:12 ]
; CHECK-NEXT:  ld a,e
; CHECK-NEXT:  ld (hl),a
; CHECK-NEXT:  C$twice.zig$3$0_0$0 = .
; CHECK-NEXT:  .globl C$twice.zig$3$0_0$0
; CHECK-NEXT:  ; src/twice.zig:3:5 @[ src/math.zig:17:12 ]
; CHECK-NEXT:  ld a,b
; CHECK-NEXT:  ld (hl),a
; CHECK-NEXT:  C$math.zig$17$0_0$1 = .
; CHECK-NEXT:  .globl C$math.zig$17$0_0$1
; CHECK-NEXT:  ; src/math.zig:17:30

; Every caller of nested inlined code gets a symbol, the outermost first.
; A file name keeps no folders, and a byte that sdas rejects in a symbol
; becomes `_`.

; CHECK-LABEL: _nested:
; CHECK:       C$math.zig$21$0_0$0 = .
; CHECK-NEXT:  .globl C$math.zig$21$0_0$0
; CHECK-NEXT:  C$twice.zig$2$0_0$4 = .
; CHECK-NEXT:  .globl C$twice.zig$2$0_0$4
; CHECK-NEXT:  C$my_file_2.zig$7$0_0$0 = .
; CHECK-NEXT:  .globl C$my_file_2.zig$7$0_0$0
; CHECK-NEXT:  ; src/my-file 2.zig:7:3 @[ src/twice.zig:2:14 @[ src/math.zig:21:5 ] ]

; The symbol of a function with local linkage names the module, which
; takes the name of the source file without its extension.

; CHECK-LABEL: _hidden:
; CHECK:       Fmy_game_main$hidden$0$0 = .
; CHECK-NEXT:  .globl Fmy_game_main$hidden$0$0

; `$` separates the fields of a symbol, so a function symbol can't hold the
; `$28` that the label has for `(`.

; CHECK-LABEL: _List$28u8$29.len:
; CHECK:       G$List_u8_.len$0$0 = .
; CHECK-NEXT:  .globl G$List_u8_.len$0$0

; A function without debug information gets a function symbol and no line
; symbols.

; CHECK-LABEL: _plain:
; CHECK:       G$plain$0$0 = .
; CHECK-NEXT:  .globl G$plain$0$0
; CHECK-NOT:   C$
; CHECK:       ret

source_filename = "src/my-game.main.zig"

define i8 @add(i8 %a, i8 %b) !dbg !5 {
  %sum = add i8 %a, %b, !dbg !9
    #dbg_value(i8 %sum, !13, !DIExpression(), !14)
  ; Line 0 means no line, so the instruction stays with line 3.
  %flipped = xor i8 %sum, 1, !dbg !15
  ; Another column of line 3 is still line 3.
  %more = add i8 %flipped, %b, !dbg !16
  %doubled = add i8 %more, %more, !dbg !11
  ret i8 %doubled, !dbg !12
}

define void @same() !dbg !17 {
  ret void, !dbg !18
}

define i8 @opening(i8 %a) !dbg !20 {
  %next = add i8 %a, 1
  ret i8 %next, !dbg !21
}

@out = global i8 0

define void @two_calls(i8 %a, i8 %b) !dbg !22 {
  store volatile i8 %a, ptr @out, !dbg !25
  store volatile i8 %b, ptr @out, !dbg !26
  ret void, !dbg !27
}

define void @around_call(i8 %a, i8 %b) !dbg !28 {
  store volatile i8 %a, ptr @out, !dbg !29
  store volatile i8 %b, ptr @out, !dbg !30
  store volatile i8 %a, ptr @out, !dbg !31
  store volatile i8 %b, ptr @out, !dbg !32
  ret void, !dbg !33
}

define void @nested(i8 %a) !dbg !34 {
  store volatile i8 %a, ptr @out, !dbg !39
  ret void, !dbg !40
}

define internal void @hidden() !dbg !41 {
  ret void, !dbg !42
}

define void @"List(u8).len"() !dbg !43 {
  ret void, !dbg !44
}

define void @plain() {
  ret void
}

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3, !4}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "test", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug)
!1 = !DIFile(filename: "src/math.zig", directory: "/project")
!2 = !DIFile(filename: "src/twice.zig", directory: "/project")
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = !{i32 2, !"Dwarf Version", i32 4}
!5 = distinct !DISubprogram(name: "add", scope: !1, file: !1, line: 2, type: !6, scopeLine: 2, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!6 = !DISubroutineType(types: !7)
!7 = !{null}
!8 = distinct !DISubprogram(name: "twice", scope: !2, file: !2, line: 1, type: !6, scopeLine: 1, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!9 = !DILocation(line: 3, column: 17, scope: !5)
!10 = !DILocation(line: 4, column: 12, scope: !5)
!11 = !DILocation(line: 2, column: 14, scope: !8, inlinedAt: !10)
!12 = !DILocation(line: 5, column: 5, scope: !5)
!13 = !DILocalVariable(name: "sum", scope: !5, file: !1, line: 9, type: !19)
!14 = !DILocation(line: 9, column: 9, scope: !5)
!15 = !DILocation(line: 0, scope: !5)
!16 = !DILocation(line: 3, column: 30, scope: !5)
!17 = distinct !DISubprogram(name: "same", scope: !1, file: !1, line: 5, type: !6, scopeLine: 5, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!18 = !DILocation(line: 5, column: 5, scope: !17)
!19 = !DIBasicType(name: "u8", size: 8, encoding: DW_ATE_unsigned)
!20 = distinct !DISubprogram(name: "opening", scope: !1, file: !1, line: 8, type: !6, scopeLine: 8, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!21 = !DILocation(line: 9, column: 5, scope: !20)
!22 = distinct !DISubprogram(name: "two_calls", scope: !1, file: !1, line: 11, type: !6, scopeLine: 11, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!23 = !DILocation(line: 12, column: 9, scope: !22)
!24 = !DILocation(line: 13, column: 9, scope: !22)
!25 = !DILocation(line: 2, column: 14, scope: !8, inlinedAt: !23)
!26 = !DILocation(line: 2, column: 14, scope: !8, inlinedAt: !24)
!27 = !DILocation(line: 14, column: 5, scope: !22)
!28 = distinct !DISubprogram(name: "around_call", scope: !1, file: !1, line: 16, type: !6, scopeLine: 16, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!29 = !DILocation(line: 17, column: 5, scope: !28)
!30 = !DILocation(line: 2, column: 14, scope: !8, inlinedAt: !45)
!31 = !DILocation(line: 3, column: 5, scope: !8, inlinedAt: !45)
!32 = !DILocation(line: 17, column: 30, scope: !28)
!33 = !DILocation(line: 18, column: 5, scope: !28)
!34 = distinct !DISubprogram(name: "nested", scope: !1, file: !1, line: 20, type: !6, scopeLine: 20, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!35 = !DIFile(filename: "src/my-file 2.zig", directory: "/project")
!36 = distinct !DISubprogram(name: "inner", scope: !35, file: !35, line: 1, type: !6, scopeLine: 1, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!37 = !DILocation(line: 21, column: 5, scope: !34)
!38 = !DILocation(line: 2, column: 14, scope: !8, inlinedAt: !37)
!39 = !DILocation(line: 7, column: 3, scope: !36, inlinedAt: !38)
!40 = !DILocation(line: 22, column: 5, scope: !34)
!41 = distinct !DISubprogram(name: "hidden", scope: !1, file: !1, line: 24, type: !6, scopeLine: 24, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!42 = !DILocation(line: 25, column: 5, scope: !41)
!43 = distinct !DISubprogram(name: "List(u8).len", scope: !1, file: !1, line: 27, type: !6, scopeLine: 27, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!44 = !DILocation(line: 28, column: 5, scope: !43)
!45 = !DILocation(line: 17, column: 12, scope: !28)
