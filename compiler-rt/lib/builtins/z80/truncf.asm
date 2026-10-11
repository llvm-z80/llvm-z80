; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
    .area   _CODE
    .globl  _truncf
;===------------------------------------------------------------------------===;
;@ float truncf(float x)
;@     ensures
;@         same(result, truncf(x));
;@     tests
;@         samples 1500000000;
;===------------------------------------------------------------------------===;
_truncf:
    ld      a, l
    rlca
    ld      a, h
    rla                         ; A = exponent
    cp      #150
    ret     nc                  ; |x| >= 2^23, infinity or NaN
    sub     #127
    jr      c, __truncf_zero    ; |x| < 1
    ld      c, a                ; C = number of integer bits in the mantissa
    ; The boundary between integer and fraction bits falls in one byte.
    cpl
    and     #7
    ld      b, a            ; B = number of fraction bits in that byte
    ld      a, #0xFF
    jr      z, __truncf_clear
__truncf_mask:
    add     a, a
    djnz    __truncf_mask
__truncf_clear:
    ld      b, a            ; B = the integer bits of that byte
    ld      a, c
    cp      #16
    jr      nc, __truncf_e
    ld      e, #0
    cp      #8
    jr      nc, __truncf_d
    ld      d, #0
    ld      a, b
    and     l
    ld      l, a
    ret
__truncf_d:
    ld      a, b
    and     d
    ld      d, a
    ret
__truncf_e:
    ld      a, b
    and     e
    ld      e, a
    ret
__truncf_zero:
    ld      a, h
    and     #0x80
    ld      h, a
    xor     a
    ld      l, a
    ld      d, a
    ld      e, a
    ret
