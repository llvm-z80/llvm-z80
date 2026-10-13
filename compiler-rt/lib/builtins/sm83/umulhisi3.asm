; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
    .area   _CODE
    .globl  ___umulhisi3
;===------------------------------------------------------------------------===;
;@ uint32_t __umulhisi3(uint16_t a, uint16_t b)
;@     ensures
;@         result == (uint32_t)a * b;
;@     tests
;@         exhaustive;
;===------------------------------------------------------------------------===;
___umulhisi3:
    xor     a
    ld      h, a
    ld      l, a
    or      d
    ld      a, #16
    jr      nz, ___umulhisi3_loop
    ld      d, e            ; a below 256 takes 8 steps
    ld      e, h
    ld      a, #8
    ; MSB-first shift-and-add over DE:HL: a in DE shifts out as the product
    ; shifts in.
___umulhisi3_loop:
    add     hl, hl
    rl      e
    rl      d
    jr      nc, ___umulhisi3_next
    add     hl, bc
    jr      nc, ___umulhisi3_next
    inc     de
___umulhisi3_next:
    dec     a
    jr      nz, ___umulhisi3_loop
    ld      c, l
    ld      b, h
    ret
