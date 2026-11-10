; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
    .area   _CODE
    .globl  _roundf
;===------------------------------------------------------------------------===;
;@ float roundf(float x)
;@     ensures
;@         same(result, roundf(x));
;@     tests
;@         samples 1500000000;
;===------------------------------------------------------------------------===;
_roundf:
    ld      a, l
    rlca
    ld      a, h
    rla                         ; A = exponent
    cp      #150
    ret     nc                  ; |x| >= 2^23, infinity or NaN
    sub     #126
    jr      c, __roundf_zero    ; |x| < 0.5
    jr      z, __roundf_one     ; 0.5 <= |x| < 1
    cpl
    add     a, #24              ; A = number of fraction bits - 1
    ; Move whole fraction bytes out of E.
__roundf_byte:
    cp      #8
    jr      c, __roundf_bit
    ld      e, d
    ld      d, l
    ld      l, h
    ld      h, #0
    sub     #8
    jr      __roundf_byte
__roundf_bit:
    ld      c, #1
    or      a
    jr      z, __roundf_round
__roundf_shift:
    sla     c
    dec     a
    jr      nz, __roundf_shift
__roundf_round:
    ; C = the one-half bit of E.
    ld      a, c
    add     a, a
    dec     a
    ld      b, a                ; B = the fraction bits of E
    ld      a, e
    and     c
    jr      z, __roundf_down    ; below one half
    ld      a, e
    or      b
    inc     a                   ; clear the fraction and add one
    ld      e, a
    jr      nz, __roundf_back
    inc     d
    jr      nz, __roundf_back
    inc     l
    jr      nz, __roundf_back
    inc     h
    jr      __roundf_back
__roundf_down:
    ld      a, b
    cpl
    and     e
    ld      e, a
__roundf_back:
    ; Move the bytes back, filling the fraction bytes with zero.
    ld      a, h
    or      a
    ret     nz
    ld      h, l
    ld      l, d
    ld      d, e
    ld      e, a
    jr      __roundf_back
__roundf_one:
    ld      l, #0x80        ; H is already right for 1.0
    ld      de, #0
    ret
__roundf_zero:
    ld      a, h
    and     #0x80
    ld      h, a
    xor     a
    ld      l, a
    ld      d, a
    ld      e, a
    ret
