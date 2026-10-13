; RUN: split-file %s %t
; RUN: llc -mtriple=z80 -z80-asm-format=z88dk -verify-machineinstrs -o - %t/invalid.ll 2>&1 | FileCheck %s --check-prefix=BADFILE
; RUN: llc -mtriple=z80 -verify-machineinstrs -o - %t/invalid.ll | FileCheck %s --check-prefix=ELF
; RUN: llc -mtriple=z80 -z80-asm-format=z88dk -verify-machineinstrs -o - %t/valid.ll | FileCheck %s --check-prefix=VALID

; Bad filenames: C_LINE is skipped with a warning (optional debug info, not a build error).
; BADFILE-COUNT-3: warning: C_LINE skipped: filename contains quotes or line breaks
; BADFILE-NOT: warning:
; BADFILE-NOT: C_LINE

; Bad function names: degrade to "file" form only (no ::func appended).
; BADFILE: C_LINE 2, "test.c"
; BADFILE: C_LINE 2, "test.c"
; BADFILE: C_LINE 2, "test.c"

; ELF format must NOT emit C_LINE directives.
; ELF-NOT: C_LINE

; Preserve raw backslashes, spaces and UTF-8 bytes in the file::func form.
; VALID: C_LINE 2, "path\with spaceé.c::example"
; VALID-LABEL: _line_zero:
; VALID-NOT: C_LINE
; VALID: ret
; VALID-NOT: C_LINE

;--- invalid.ll
define void @file_quote() !dbg !10 {
  ret void, !dbg !20
}
define void @file_lf() !dbg !11 {
  ret void, !dbg !21
}
define void @file_cr() !dbg !12 {
  ret void, !dbg !22
}
define void @function_quote() !dbg !13 {
  ret void, !dbg !23
}
define void @function_lf() !dbg !14 {
  ret void, !dbg !24
}
define void @function_cr() !dbg !15 {
  ret void, !dbg !25
}

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!5}
!0 = distinct !DICompileUnit(language: DW_LANG_C11, file: !1, producer: "test", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!1 = !DIFile(filename: "test.c", directory: ".")
!2 = !DISubroutineType(types: !{})
!5 = !{i32 2, !"Debug Info Version", i32 3}
!6 = !DIFile(filename: "bad\22name.c", directory: ".")
!7 = !DIFile(filename: "bad\0Aname.c", directory: ".")
!8 = !DIFile(filename: "bad\0Dname.c", directory: ".")
!10 = distinct !DISubprogram(name: "file_quote", scope: !6, file: !6, line: 1, type: !2, unit: !0)
!11 = distinct !DISubprogram(name: "file_lf", scope: !7, file: !7, line: 1, type: !2, unit: !0)
!12 = distinct !DISubprogram(name: "file_cr", scope: !8, file: !8, line: 1, type: !2, unit: !0)
!13 = distinct !DISubprogram(name: "bad\22function", scope: !1, file: !1, line: 1, type: !2, unit: !0)
!14 = distinct !DISubprogram(name: "bad\0Afunction", scope: !1, file: !1, line: 1, type: !2, unit: !0)
!15 = distinct !DISubprogram(name: "bad\0Dfunction", scope: !1, file: !1, line: 1, type: !2, unit: !0)
!20 = !DILocation(line: 2, scope: !10)
!21 = !DILocation(line: 2, scope: !11)
!22 = !DILocation(line: 2, scope: !12)
!23 = !DILocation(line: 2, scope: !13)
!24 = !DILocation(line: 2, scope: !14)
!25 = !DILocation(line: 2, scope: !15)

;--- valid.ll
define void @example() !dbg !3 {
  ret void, !dbg !4
}
define void @line_zero() !dbg !7 {
  ret void, !dbg !8
}
!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!5}
!0 = distinct !DICompileUnit(language: DW_LANG_C11, file: !1, producer: "test", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!1 = !DIFile(filename: "path\5Cwith space\C3\A9.c", directory: ".")
!2 = !DISubroutineType(types: !{})
!3 = distinct !DISubprogram(name: "example", scope: !1, file: !1, line: 1, type: !2, unit: !0)
!4 = !DILocation(line: 2, scope: !3)
!5 = !{i32 2, !"Debug Info Version", i32 3}
!6 = !DIFile(filename: "bad\22name.c", directory: ".")
!7 = distinct !DISubprogram(name: "line_zero", scope: !6, file: !6, line: 1, type: !2, unit: !0)
!8 = !DILocation(line: 0, scope: !7)
