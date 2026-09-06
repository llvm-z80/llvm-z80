; RUN: llc -verify-machineinstrs -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s --check-prefix=Z80
; RUN: llc -verify-machineinstrs -mtriple=sm83 -O1 < %s | FileCheck %s --check-prefix=SM83

; Measured at Os over the suite: a switch of up to seven cases compiles
; smaller on SM83 as a comparison tree, while Z80's size optimum is the
; generic threshold of four. Only SM83 raises the minimum, to eight.

@g = external global i8

; Z80-LABEL: five_way:
; Z80: LJTI
; SM83-LABEL: five_way:
; SM83-NOT: LJTI
define void @five_way(i8 %x) {
entry:
  switch i8 %x, label %done [
    i8 0, label %c0
    i8 1, label %c1
    i8 2, label %c2
    i8 3, label %c3
    i8 4, label %c4
  ]
c0:
  store volatile i8 10, ptr @g
  br label %done
c1:
  store volatile i8 21, ptr @g
  br label %done
c2:
  store volatile i8 32, ptr @g
  br label %done
c3:
  store volatile i8 43, ptr @g
  br label %done
c4:
  store volatile i8 54, ptr @g
  br label %done
done:
  ret void
}

; Eight cases reach SM83's threshold again.
; SM83-LABEL: eight_way:
; SM83: LJTI
define void @eight_way(i8 %x) {
entry:
  switch i8 %x, label %done [
    i8 0, label %c0
    i8 1, label %c1
    i8 2, label %c2
    i8 3, label %c3
    i8 4, label %c4
    i8 5, label %c5
    i8 6, label %c6
    i8 7, label %c7
  ]
c0:
  store volatile i8 10, ptr @g
  br label %done
c1:
  store volatile i8 21, ptr @g
  br label %done
c2:
  store volatile i8 32, ptr @g
  br label %done
c3:
  store volatile i8 43, ptr @g
  br label %done
c4:
  store volatile i8 54, ptr @g
  br label %done
c5:
  store volatile i8 65, ptr @g
  br label %done
c6:
  store volatile i8 76, ptr @g
  br label %done
c7:
  store volatile i8 87, ptr @g
  br label %done
done:
  ret void
}
