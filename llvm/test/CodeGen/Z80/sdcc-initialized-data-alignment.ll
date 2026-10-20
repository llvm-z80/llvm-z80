; RUN: not llc -mtriple=sm83-unknown-none-sdcc < %s -o /dev/null 2>&1 | FileCheck %s

@counter = global i8 42, align 2

; CHECK: error: SDCC initialized data requires alignment 1
