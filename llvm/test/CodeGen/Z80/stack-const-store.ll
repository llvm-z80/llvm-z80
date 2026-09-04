; RUN: llc -verify-machineinstrs -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s --check-prefix=Z80
; RUN: llc -verify-machineinstrs -mtriple=sm83 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s --check-prefix=SM83
;
; A 16-bit constant stored to a stack slot goes to memory a half at a time
; instead of through a register pair, which is shorter and leaves the pair
; free for allocation.
;
; A pair costs three bytes to materialize against one more per immediate
; store, so a second use of the constant pays for it. That is a byte saved
; for a few cycles spent, which the size levels take and the others do not.
; SM83 pays for its own address setup on every frame access and gets nothing
; back from the pair, so it folds either way.

declare void @use(ptr)

define void @single_store() "frame-pointer"="all" {
  %p = alloca i16
  store volatile i16 4660, ptr %p
  call void @use(ptr %p)
  ret void
}

define void @shared_constant() optsize "frame-pointer"="all" {
  %p = alloca i16
  %q = alloca i16
  store volatile i16 4660, ptr %p
  store volatile i16 4660, ptr %q
  call void @use(ptr %p)
  ret void
}

define void @shared_constant_fast() "frame-pointer"="all" {
  %p = alloca i16
  %q = alloca i16
  store volatile i16 4660, ptr %p
  store volatile i16 4660, ptr %q
  call void @use(ptr %p)
  ret void
}

; Z80-LABEL: _single_store:
; Z80-NOT:     ld bc,#4660
; Z80:         ld -2(ix),#52
; Z80-NEXT:    ld -1(ix),#18

; Z80-LABEL: _shared_constant:
; Z80:         ld bc,#4660
; Z80-NEXT:    ld -2(ix),c
; Z80-NEXT:    ld -1(ix),b
; Z80-NEXT:    ld -4(ix),c
; Z80-NEXT:    ld -3(ix),b

; Z80-LABEL: _shared_constant_fast:
; Z80-NOT:     ld bc,#4660
; Z80:         ld -2(ix),#52
; Z80-NEXT:    ld -1(ix),#18

; SM83-LABEL: _single_store:
; SM83:        ld (hl),#52
; SM83-NEXT:   inc hl
; SM83-NEXT:   ld (hl),#18

; SM83-LABEL: _shared_constant:
; SM83-NOT:    ld bc,#4660
; SM83:        ld a,#52
; SM83-NEXT:   ld (hl+),a
; SM83-NEXT:   ld (hl),#18
