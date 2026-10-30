; RUN: llc -verify-machineinstrs -mtriple=sm83-unknown-none-sdcc < %s | FileCheck %s
; RUN: llc -verify-machineinstrs -mtriple=sm83 < %s | FileCheck %s --check-prefix=ELF
;
; SDCC output writes each byte that sdas rejects, and '$' itself, as '$' plus
; two hex digits. ELF output keeps the original names and quotes them.

; an internal function with parentheses, both defined and called
define internal void @"f(u8)"() {
  ret void
}
; CHECK: _f$28u8$29:
; ELF: "_f(u8)":

define void @caller() {
  call void @"f(u8)"()
  ret void
}
; CHECK: call _f$28u8$29
; ELF: call "_f(u8)"

; an external declaration
declare void @"ext(fn)"()
; an external declaration has no label

define void @ext_caller() {
  call void @"ext(fn)"()
  ret void
}
; CHECK: call _ext$28fn$29
; ELF: call "_ext(fn)"

; a private global, whose L_ prefix comes from the Mangler
@"p(x)" = private global i16 6
; CHECK: L_p$28x$29:
; ELF: "L_p(x)":

; a pair that would collide without $ escaping, @"a(b" and @"a$28b"
@"a(b" = internal global i16 1
; CHECK: _a$28b:
; ELF: "_a(b":

@"a$28b" = internal global i16 2
; CHECK: _a$2428b:
; ELF: _a$28b:

; an alias
@"al(x)" = internal alias i16, ptr @"a(b"
; CHECK: _al$28x$29 = _a$28b
; ELF: "_al(x)" = "_a(b"
