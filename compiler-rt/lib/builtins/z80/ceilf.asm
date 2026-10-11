; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
    .area   _CODE
    .globl  _ceilf
;===------------------------------------------------------------------------===;
;@ float ceilf(float x)
;@     ensures
;@         same(result, ceilf(x));
;@     tests
;@         samples 1500000000;
;===------------------------------------------------------------------------===;
_ceilf:
    ld      b, h                ; B = sign
    ld      a, l
    rlca
    ld      a, h
    rla                         ; A = exponent
    cp      #150
    ret     nc                  ; |x| >= 2^23, infinity or NaN
    sub     #127
    jr      c, __ceilf_small    ; |x| < 1
    cpl
    add     a, #23              ; A = number of fraction bits - 1
    ; Move whole fraction bytes out of E, setting bit 0 of E for a nonzero one.
__ceilf_byte:
    cp      #8
    jr      c, __ceilf_bit
    ld      c, a
    ld      a, e
    or      a
    ld      e, d
    ld      d, l
    ld      l, h
    ld      h, #0
    jr      z, __ceilf_next
    set     0, e
__ceilf_next:
    ld      a, c
    sub     #8
    jr      __ceilf_byte
__ceilf_bit:
    ld      c, a
    inc     c
    ld      a, #0xFF
__ceilf_mask:
    add     a, a
    dec     c
    jr      nz, __ceilf_mask
    ld      c, a                ; C = the integer bits of E
    bit     7, b
    jr      nz, __ceilf_down    ; x < 0
    cpl
    and     e
    jr      z, __ceilf_back     ; no fraction
    ld      a, c
    cpl
    or      e
    inc     a                   ; clear the fraction and add one
    ld      e, a
    jr      nz, __ceilf_back
    inc     d
    jr      nz, __ceilf_back
    inc     l
    jr      nz, __ceilf_back
    inc     h
    jr      __ceilf_back
__ceilf_down:
    and     e
    ld      e, a
__ceilf_back:
    ; Move the bytes back, filling the fraction bytes with zero.
    ld      a, h
    or      a
    ret     nz
    ld      h, l
    ld      l, d
    ld      d, e
    ld      e, a
    jr      __ceilf_back
__ceilf_small:
    bit     7, b
    jr      nz, __ceilf_zero    ; -1 < x <= -0
    ld      a, h
    or      l
    or      d
    or      e
    jr      z, __ceilf_zero     ; +0
    ld      hl, #0x3F80         ; 0 < x < 1
    ld      de, #0
    ret
__ceilf_zero:
    ld      a, h
    and     #0x80
    ld      h, a
    xor     a
    ld      l, a
    ld      d, a
    ld      e, a
    ret
