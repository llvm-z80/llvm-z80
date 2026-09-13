; RUN: llc -verify-machineinstrs -mtriple=sm83 -mcpu=generic -filetype=obj < %s -o /dev/null
; RUN: llc -verify-machineinstrs -mtriple=sm83 -filetype=obj < %s -o /dev/null
;
; A driver that names no CPU may send "generic" rather than nothing. Both
; readings have to reach the triple's default, inline assembly included.

define void @sm83_only_asm() {
  call void asm sideeffect "ldh (0x44),a", ""()
  call void asm sideeffect "ld (hl+),a", ""()
  call void asm sideeffect "ldhl sp,4", ""()
  ret void
}
