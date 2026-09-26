; RUN: llc -mtriple=z80 -O2 -stop-after=instruction-select < %s | FileCheck %s --check-prefix=MIR
; RUN: llc -verify-machineinstrs -mtriple=z80 -O2 < %s | FileCheck %s
;
; IN r,(C) and OUT (C),r take any register, so the byte read or written
; lives wherever the allocator puts it rather than passing through A.

declare i8 @llvm.z80.in(i8)
declare void @llvm.z80.out(i8, i8)

; MIR-LABEL: name: echo
; MIR:       [[V:%[0-9]+]]:gr8 = IN8_C
; MIR:       OUT8_C [[V]]
; CHECK-LABEL: _echo:
; CHECK:       ld c,a
; CHECK-NEXT:  in [[R:[a-z]]],(c)
; CHECK-NEXT:  out (c),[[R]]
define void @echo(i8 %port) {
  %v = call i8 @llvm.z80.in(i8 %port)
  call void @llvm.z80.out(i8 %port, i8 %v)
  ret void
}
