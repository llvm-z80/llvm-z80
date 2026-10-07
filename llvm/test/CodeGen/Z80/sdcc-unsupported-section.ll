; RUN: not llc -mtriple=sm83-unknown-none-sdcc < %s -o /dev/null 2>&1 | FileCheck %s

@tbl = constant [2 x i8] [i8 7, i8 8], align 1, section "custom"

; CHECK: LLVM ERROR: Cannot map section 'custom' to an sdasz80 area. If you want to use a custom name, prepend it with an underscore.
