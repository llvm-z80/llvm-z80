; RUN: not llc -mtriple=z80 -O2 < %s 2>&1 | FileCheck %s --check-prefix=Z80
; RUN: not llc -mtriple=sm83 -O2 < %s 2>&1 | FileCheck %s --check-prefix=SM83
;
; A body saves a listed register with its pair, so it cannot keep a half
; whose other half returns the result. Z80 returns an i8 in L here and SM83
; in E. A naked body is taken as written.

declare void @plain()

; Z80: error: {{.*}}in function keep_h{{.*}}cannot keep h for callers, since restoring hl would overwrite the result
define z80_sdcccall0 i8 @keep_h() "z80-preserves-regs"="h" {
  call void @plain()
  ret i8 3
}

; SM83: error: {{.*}}in function keep_d{{.*}}cannot keep d for callers, since restoring de would overwrite the result
define z80_sdcccall0 i8 @keep_d() "z80-preserves-regs"="d" {
  call void @plain()
  ret i8 3
}

; Z80-NOT: error
; SM83-NOT: error
define z80_sdcccall0 i8 @naked_h() naked "z80-preserves-regs"="h,d" {
  call void asm sideeffect "ld l,#3\0Aret", ""()
  unreachable
}
