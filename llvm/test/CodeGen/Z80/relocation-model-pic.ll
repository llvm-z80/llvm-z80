; RUN: llc -mtriple=z80 -relocation-model=pic < %s \
; RUN:   | FileCheck %s --check-prefix=ELF --implicit-check-not=.data
; RUN: llc -mtriple=sm83 -relocation-model=pic < %s \
; RUN:   | FileCheck %s --check-prefix=ELF --implicit-check-not=.data
; RUN: llc -mtriple=z80-unknown-none-sdcc -relocation-model=pic < %s \
; RUN:   | FileCheck %s --check-prefix=SDCC --implicit-check-not=_DATA
; RUN: llc -mtriple=sm83-unknown-none-sdcc -relocation-model=pic < %s \
; RUN:   | FileCheck %s --check-prefix=SDCC --implicit-check-not=_DATA

; A PIC request is treated as static. A jump table holds absolute addresses,
; and a constant table of pointers stays in read-only data.

@a = private constant [2 x i8] c"a\00"
@b = private constant [2 x i8] c"b\00"

; ELF-LABEL:  LJTI0_0:
; ELF-NEXT:   .short .LBB0_{{[0-9]+}}{{$}}
; SDCC-LABEL: LJTI0_0:
; SDCC-NEXT:  .dw .LBB0_{{[0-9]+}}{{$}}
define i16 @pick(i16 %x) {
  switch i16 %x, label %d [
    i16 0, label %c0
    i16 1, label %c1
    i16 2, label %c2
    i16 3, label %c3
    i16 4, label %c4
    i16 5, label %c5
    i16 6, label %c6
    i16 7, label %c7
  ]
c0:
  ret i16 10
c1:
  ret i16 21
c2:
  ret i16 32
c3:
  ret i16 43
c4:
  ret i16 54
c5:
  ret i16 65
c6:
  ret i16 76
c7:
  ret i16 87
d:
  ret i16 0
}

; ELF:  _names:
; SDCC: _names:
@names = constant [2 x ptr] [ptr @a, ptr @b]
