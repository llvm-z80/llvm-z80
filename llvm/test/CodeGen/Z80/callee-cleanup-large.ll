; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefix=Z80
; RUN: llc -mtriple=sm83 -z80-asm-format=sdasz80 -O1 -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefix=SM83
;
; Callee-cleanup returns whose argument block is too large for the compact
; sequences.  Neither target can drop an arbitrary number of bytes in one
; instruction: Z80 has no `ret N` at all, and SM83's `add sp,e` takes a SIGNED
; 8-bit displacement, so a cleanup of 128 or more has to be split.  Feeding it
; 128 as a byte would move SP down 128 instead of up, and 256 would clean up
; nothing at all.
;
; A byval aggregate is the compact way to demand a big argument block: the
; whole thing is pushed, and cc133 (__smallc __z88dk_callee) makes the callee
; pop it.

target datalayout = "e-p:16:8-i16:8-i32:8-i64:8-a:8-n8:16"

%Big = type [130 x i8]
%Small = type [24 x i8]

; 130 bytes, and nothing in the return registers.
define cc133 void @big_byval(ptr byval(%Big) %p) {
; Z80 builds the new SP in HL, which is free here.
; Z80-LABEL: _big_byval:
; Z80:         pop bc
; Z80-NEXT:    ld hl,#130
; Z80-NEXT:    add hl,sp
; Z80-NEXT:    ld sp,hl
; Z80-NEXT:    push bc
; Z80-NEXT:    ret
;
; SM83 has no such sequence and must split the add; 127 + 3 = 130.
; SM83-LABEL: _big_byval:
; SM83:        pop hl
; SM83-NEXT:   add sp,#127
; SM83-NEXT:   add sp,#3
; SM83-NEXT:   jp (hl)
  %v = load i8, ptr %p
  store i8 %v, ptr inttoptr(i16 16384 to ptr)
  ret void
}

; The same block, but now the return value occupies HL, so neither target may
; use it as the scratch that carries the new stack pointer or the return
; address.
define cc133 i32 @big_byval_wide(ptr byval(%Big) %p) {
; Z80 falls back to IY, which every call already declares clobbered.
; Z80-LABEL: _big_byval_wide:
; Z80:         pop bc
; Z80-NEXT:    ld iy,#130
; Z80-NEXT:    add iy,sp
; Z80-NEXT:    ld sp,iy
; Z80-NEXT:    push bc
; Z80-NEXT:    ret
;
; SM83's only indirect jump goes through HL, so the return address rides in BC
; and the split add still applies.
; SM83-LABEL: _big_byval_wide:
; SM83:        pop bc
; SM83-NEXT:   add sp,#127
; SM83-NEXT:   add sp,#3
; SM83-NEXT:   push bc
; SM83-NEXT:   ret
  %v = load i8, ptr %p
  %z = zext i8 %v to i32
  ret i32 %z
}

; Below the boundary the SM83 cleanup stays a single add, so the split above is
; not something it does unconditionally.
define cc133 i32 @small_byval_wide(ptr byval(%Small) %p) {
; SM83-LABEL: _small_byval_wide:
; SM83:        pop bc
; SM83-NEXT:   add sp,#24
; SM83-NEXT:   push bc
; SM83-NEXT:   ret
  %v = load i8, ptr %p
  %z = zext i8 %v to i32
  ret i32 %z
}
