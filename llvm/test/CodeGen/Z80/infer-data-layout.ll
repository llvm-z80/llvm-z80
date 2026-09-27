;; A module without a data layout takes it from the triple. The backend and
;; clang read the same string, so all three must agree.
; RUN: opt -mtriple=z80 -S -passes=no-op-module < %s | FileCheck %s
; RUN: opt -mtriple=sm83 -S -passes=no-op-module < %s | FileCheck %s

target datalayout = ""
; CHECK: target datalayout = "e-m:o-p:16:8-i16:8-i32:8-i64:8-i128:8-f16:8-f32:8-f64:8-f128:8-ve-a:8-n8:16"
