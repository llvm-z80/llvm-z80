; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
    .area   _CODE
    .globl  _floorf
;===------------------------------------------------------------------------===;
;@ float floorf(float x)
;@     ensures
;@         same(result, floorf(x));
;@     tests
;@         samples 1500000000;
;===------------------------------------------------------------------------===;
_floorf:
    ld      h, d                ; H = sign
    ld      a, e
    rlca
    ld      a, d
    rla                         ; A = exponent
    cp      #150
    ret     nc                  ; |x| >= 2^23, infinity or NaN
    sub     #127
    jr      c, __floorf_small   ; |x| < 1
    cpl
    add     a, #23              ; A = number of fraction bits - 1
    ; Move whole fraction bytes out of C, setting bit 0 of C for a nonzero one.
__floorf_byte:
    cp      #8
    jr      c, __floorf_bit
    ld      l, a
    ld      a, c
    or      a
    ld      c, b
    ld      b, e
    ld      e, d
    ld      d, #0
    jr      z, __floorf_next
    set     0, c
__floorf_next:
    ld      a, l
    sub     #8
    jr      __floorf_byte
__floorf_bit:
    ld      l, a
    inc     l
    ld      a, #0xFF
__floorf_mask:
    add     a, a
    dec     l
    jr      nz, __floorf_mask
    ld      l, a                ; L = the integer bits of C
    bit     7, h
    jr      z, __floorf_down    ; x > 0
    cpl
    and     c
    jr      z, __floorf_back    ; no fraction
    ld      a, l
    cpl
    or      c
    inc     a                   ; clear the fraction and add one
    ld      c, a
    jr      nz, __floorf_back
    inc     b
    jr      nz, __floorf_back
    inc     e
    jr      nz, __floorf_back
    inc     d
    jr      __floorf_back
__floorf_down:
    and     c
    ld      c, a
__floorf_back:
    ; Move the bytes back, filling the fraction bytes with zero.
    ld      a, d
    or      a
    ret     nz
    ld      d, e
    ld      e, b
    ld      b, c
    ld      c, a
    jr      __floorf_back
__floorf_small:
    bit     7, h
    jr      z, __floorf_zero    ; 0 <= x < 1
    ld      a, d
    add     a, a
    or      e
    or      b
    or      c
    jr      z, __floorf_zero    ; -0
    ld      de, #0xBF80         ; -1 < x < 0
    ld      bc, #0
    ret
__floorf_zero:
    ld      a, d
    and     #0x80
    ld      d, a
    xor     a
    ld      e, a
    ld      b, a
    ld      c, a
    ret
