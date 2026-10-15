; RUN: not llc -mtriple=sm83-unknown-none-sdcc < %s -o /dev/null 2>&1 | FileCheck %s

@tbl = constant [2 x i8] [i8 7, i8 8], align 1, section "custom"

; CHECK: error: section 'custom' cannot be emitted in the sdasz80 format; start the name with '_' to use it as an area name
; CHECK-NOT: Stack dump
