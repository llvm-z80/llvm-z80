; RUN: llc -verify-machineinstrs -mtriple=z80 -O1 < %s | FileCheck %s --check-prefix=Z80
; RUN: llc -verify-machineinstrs -mtriple=sm83 -O1 < %s | FileCheck %s --check-prefix=SM83

@g_u8 = external global i8
@g_arr = external global [8 x i8]

define i8 @load_u8() {
; Z80-LABEL: load_u8:
; Z80:         ld a,(_g_u8)
; Z80-NEXT:    ret
;
; SM83-LABEL: load_u8:
; SM83:         ld bc,_g_u8
; SM83-NEXT:    ld a,(bc)
; SM83-NEXT:    ret
  %val = load i8, ptr @g_u8
  ret i8 %val
}

define void @store_u8(i8 %v) {
; Z80-LABEL: store_u8:
; Z80:         ld (_g_u8),a
; Z80-NEXT:    ret
;
; SM83-LABEL: store_u8:
; SM83:         ld bc,_g_u8
; SM83-NEXT:    ld (bc),a
; SM83-NEXT:    ret
  store i8 %v, ptr @g_u8
  ret void
}

define i8 @load_offset() {
; Z80-LABEL: load_offset:
; Z80:         ld a,(_g_arr+3)
; Z80-NEXT:    ret
;
; SM83-LABEL: load_offset:
; SM83:         ld bc,_g_arr+3
; SM83-NEXT:    ld a,(bc)
; SM83-NEXT:    ret
  %p = getelementptr [8 x i8], ptr @g_arr, i16 0, i16 3
  %val = load i8, ptr %p
  ret i8 %val
}

define void @store_offset(i8 %v) {
; Z80-LABEL: store_offset:
; Z80:         ld (_g_arr+5),a
; Z80-NEXT:    ret
;
; SM83-LABEL: store_offset:
; SM83:         ld bc,_g_arr+5
; SM83-NEXT:    ld (bc),a
; SM83-NEXT:    ret
  %p = getelementptr [8 x i8], ptr @g_arr, i16 0, i16 5
  store i8 %v, ptr %p
  ret void
}
