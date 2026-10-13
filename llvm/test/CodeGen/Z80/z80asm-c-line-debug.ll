; Test that C_LINE directives are emitted in z80asm format when debug info is present.
; RUN: llc -verify-machineinstrs -mtriple=z80 -z80-asm-format=z88dk -O1 < %s | FileCheck %s
; RUN: llc -verify-machineinstrs -mtriple=z80 -z80-asm-format=z88dk -O1 < %s | FileCheck %s --check-prefix=NODUP
; RUN: llc -verify-machineinstrs -mtriple=z80 -O1 < %s | FileCheck %s --check-prefix=ELF

; The add instruction is the first real instruction (line 3); dbg.value at
; line 2 produces no machine instruction so C_LINE 2 is never emitted.
; Scope info is appended: "file::func".
; CHECK:     C_LINE 3, "test.c::add"
; CHECK-NEXT: add hl,de
; CHECK-NEXT: ex de,hl
; CHECK-NEXT: C_LINE 4, "test.c::add"
; CHECK-NEXT: ret

; C_LINE must not be emitted twice for the same location.
; NODUP-NOT: C_LINE 3{{.*}}C_LINE 3

; ELF format must NOT emit C_LINE directives.
; ELF-NOT: C_LINE

define i16 @add(i16 %a, i16 %b) !dbg !3 {
entry:
  call void @llvm.dbg.value(metadata i16 %a, metadata !7, metadata !DIExpression()), !dbg !9
  call void @llvm.dbg.value(metadata i16 %b, metadata !8, metadata !DIExpression()), !dbg !10
  %r = add i16 %a, %b, !dbg !11
  ret i16 %r, !dbg !12
}

declare void @llvm.dbg.value(metadata, metadata, metadata)

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!13}

!0 = distinct !DICompileUnit(language: DW_LANG_C11, file: !1, producer: "clang", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug)
!1 = !DIFile(filename: "test.c", directory: "/tmp")
!2 = !DISubroutineType(types: !{})
!3 = distinct !DISubprogram(name: "add", linkageName: "add", scope: !1, file: !1, line: 1, type: !2, unit: !0)
!4 = !DILocation(line: 2, column: 1, scope: !3)
!5 = !DILocation(line: 3, column: 1, scope: !3)
!6 = !DILocation(line: 4, column: 1, scope: !3)
!7 = !DILocalVariable(name: "a", scope: !3, file: !1, line: 1, type: !14)
!8 = !DILocalVariable(name: "b", scope: !3, file: !1, line: 1, type: !14)
!9 = !DILocation(line: 2, column: 1, scope: !3)
!10 = !DILocation(line: 2, column: 1, scope: !3)
!11 = !DILocation(line: 3, column: 1, scope: !3)
!12 = !DILocation(line: 4, column: 1, scope: !3)
!13 = !{i32 2, !"Debug Info Version", i32 3}
!14 = !DIBasicType(name: "int", size: 16, encoding: DW_ATE_signed)
