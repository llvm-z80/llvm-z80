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
    ld      a, e
    rlca
    ld      a, d
    rla                         ; A = exponent
    cp      #150
    ret     nc                  ; |x| >= 2^23, infinity or NaN
    sub     #126
    jr      c, __roundf_zero    ; |x| < 0.5
    jr      z, __roundf_one     ; 0.5 <= |x| < 1
    cpl
    add     a, #24              ; A = number of fraction bits - 1
    ; Move whole fraction bytes out of C.
__roundf_byte:
    cp      #8
    jr      c, __roundf_bit
    ld      c, b
    ld      b, e
    ld      e, d
    ld      d, #0
    sub     #8
    jr      __roundf_byte
__roundf_bit:
    ld      l, #1
    or      a
    jr      z, __roundf_round
__roundf_shift:
    sla     l
    dec     a
    jr      nz, __roundf_shift
__roundf_round:
    ; L = the one-half bit of C.
    ld      a, l
    add     a, a
    dec     a
    ld      h, a                ; H = the fraction bits of C
    ld      a, c
    and     l
    jr      z, __roundf_down    ; below one half
    ld      a, c
    or      h
    inc     a                   ; clear the fraction and add one
    ld      c, a
    jr      nz, __roundf_back
    inc     b
    jr      nz, __roundf_back
    inc     e
    jr      nz, __roundf_back
    inc     d
    jr      __roundf_back
__roundf_down:
    ld      a, h
    cpl
    and     c
    ld      c, a
__roundf_back:
    ; Move the bytes back, filling the fraction bytes with zero.
    ld      a, d
    or      a
    ret     nz
    ld      d, e
    ld      e, b
    ld      b, c
    ld      c, a
    jr      __roundf_back
__roundf_one:
    ld      e, #0x80        ; D is already right for 1.0
    ld      bc, #0
    ret
__roundf_zero:
    ld      a, d
    and     #0x80
    ld      d, a
    xor     a
    ld      e, a
    ld      b, a
    ld      c, a
    ret
