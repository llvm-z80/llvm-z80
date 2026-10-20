; RUN: llc -mtriple=z80 -O1 -verify-machineinstrs < %s | FileCheck %s --check-prefix=Z80
; RUN: llc -mtriple=sm83 -O1 -verify-machineinstrs < %s | FileCheck %s --check-prefix=SM83

; Without a frame pointer, the epilogue releases the locals with the fastest
; sequence, or the smallest under optsize, that leaves the registers of the
; return or the tail call alone.

declare i16 @g1(i16)
declare i16 @g2(i16, i16)
declare i16 @g8(i8, i8)
declare i16 @h()
declare i32 @g32()

@fp = global ptr null

; Z80-LABEL: _one:
; Z80:       call _h
; Z80-NEXT:  inc sp
; Z80-NEXT:  ret
; SM83-LABEL: _one:
; SM83:       call _h
; SM83-NEXT:  inc sp
; SM83-NEXT:  ret
define i16 @one() {
  %buf = alloca [1 x i8]
  store volatile i8 1, ptr %buf
  %r = call i16 @h()
  ret i16 %r
}

; Z80-LABEL: _two:
; Z80:       call _h
; Z80-NEXT:  pop af
; Z80-NEXT:  ret
; SM83-LABEL: _two:
; SM83:       call _h
; SM83-NEXT:  pop af
; SM83-NEXT:  ret
define i16 @two() {
  %buf = alloca [2 x i8]
  store volatile i8 1, ptr %buf
  %r = call i16 @h()
  ret i16 %r
}

; Z80-LABEL: _five:
; Z80:       call _h
; Z80-NEXT:  pop af
; Z80-NEXT:  pop af
; Z80-NEXT:  inc sp
; Z80-NEXT:  ret
; SM83-LABEL: _five:
; SM83:       call _h
; SM83-NEXT:  add sp,5
; SM83-NEXT:  ret
define i16 @five() {
  %buf = alloca [5 x i8]
  store volatile i8 1, ptr %buf
  %r = call i16 @h()
  ret i16 %r
}

; A and L hold the arguments, so BC takes the pops.
; Z80-LABEL: _bytes_in_a_l:
; Z80:       pop bc
; Z80-NEXT:  pop bc
; Z80-NEXT:  inc sp
; Z80-NEXT:  jp _g8
define i16 @bytes_in_a_l(i8 %a, i8 %b) {
  %buf = alloca [5 x i8]
  store volatile i8 1, ptr %buf
  %r = tail call i16 @g8(i8 %a, i8 %b)
  ret i16 %r
}

; The tail call reads L but not H, so only L waits in C.
; Z80-LABEL: _l_arg:
; Z80:       ld c,l
; Z80-NEXT:  ld hl,20
; Z80-NEXT:  add hl,sp
; Z80-NEXT:  ld sp,hl
; Z80-NEXT:  ld l,c
; Z80-NEXT:  jp (iy)
define i16 @l_arg(i8 %a, i8 %b) {
  %buf = alloca [20 x i8]
  store volatile i8 1, ptr %buf
  %f = load ptr, ptr @fp
  %r = tail call i16 %f(i8 %a, i8 %b)
  ret i16 %r
}

; HL holds the argument, so HL waits in DE.
; Z80-LABEL: _hl_arg:
; Z80:       ex de,hl
; Z80-NEXT:  ld hl,20
; Z80-NEXT:  add hl,sp
; Z80-NEXT:  ld sp,hl
; Z80-NEXT:  ex de,hl
; Z80-NEXT:  jp _g1
; SM83-LABEL: _hl_arg:
; SM83:       add sp,20
; SM83-NEXT:  jp _g1
define i16 @hl_arg(i16 %a) {
  %buf = alloca [20 x i8]
  store volatile i8 1, ptr %buf
  %r = tail call i16 @g1(i16 %a)
  ret i16 %r
}

; Under optsize, pops beat the DE sequence on an eight-byte frame.
; Z80-LABEL: _hl_arg_size:
; Z80:       pop af
; Z80-NEXT:  pop af
; Z80-NEXT:  pop af
; Z80-NEXT:  pop af
; Z80-NEXT:  jp _g1
define i16 @hl_arg_size(i16 %a) optsize {
  %buf = alloca [8 x i8]
  store volatile i8 1, ptr %buf
  %r = tail call i16 @g1(i16 %a)
  ret i16 %r
}

; HL and DE hold the arguments, so IY moves SP.
; Z80-LABEL: _hl_de_args:
; Z80:       ld iy,20
; Z80-NEXT:  add iy,sp
; Z80-NEXT:  ld sp,iy
; Z80-NEXT:  jp _g2
define i16 @hl_de_args(i16 %a, i16 %b) {
  %buf = alloca [20 x i8]
  store volatile i8 1, ptr %buf
  %r = tail call i16 @g2(i16 %a, i16 %b)
  ret i16 %r
}

; IY holds the target as well, so HL waits in BC.
; Z80-LABEL: _indirect:
; Z80:       pop iy
; Z80-NEXT:  ld c,l
; Z80-NEXT:  ld b,h
; Z80-NEXT:  ld hl,20
; Z80-NEXT:  add hl,sp
; Z80-NEXT:  ld sp,hl
; Z80-NEXT:  ld l,c
; Z80-NEXT:  ld h,b
; Z80-NEXT:  jp (iy)
define i16 @indirect(ptr %fp, i16 %a) {
  %buf = alloca [20 x i8]
  store volatile i8 1, ptr %buf
  %r = tail call i16 %fp(i16 %a, i16 %a)
  ret i16 %r
}

; An i32 return holds HL and DE, so IY moves SP.
; Z80-LABEL: _ret_i32:
; Z80:       ld iy,20
; Z80-NEXT:  add iy,sp
; Z80-NEXT:  ld sp,iy
; Z80-NEXT:  ret
define i32 @ret_i32() {
  %buf = alloca [20 x i8]
  store volatile i8 1, ptr %buf
  %r = call i32 @g32()
  %s = add i32 %r, 1
  ret i32 %s
}

; Z80-LABEL: _big:
; Z80:       call _h
; Z80-NEXT:  ld hl,128
; Z80-NEXT:  add hl,sp
; Z80-NEXT:  ld sp,hl
; Z80-NEXT:  ret
; SM83-LABEL: _big:
; SM83:       call _h
; SM83-NEXT:  add sp,127
; SM83-NEXT:  inc sp
; SM83-NEXT:  ret
define i16 @big() {
  %buf = alloca [128 x i8]
  store volatile i8 1, ptr %buf
  %r = call i16 @h()
  ret i16 %r
}

; SM83 jumps through HL, so ADD SP,e moves SP in steps.
; SM83-LABEL: _big_indirect:
; SM83:       add sp,127
; SM83-NEXT:  add sp,73
; SM83-NEXT:  jp (hl)
define i16 @big_indirect(ptr %fp, i16 %a) {
  %buf = alloca [200 x i8]
  store volatile i8 1, ptr %buf
  %r = tail call i16 %fp(i16 %a, i16 %a)
  ret i16 %r
}
