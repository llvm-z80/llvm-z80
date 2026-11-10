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
    ld      a, e
    rlca
    ld      a, d
    rla                         ; A = exponent
    cp      #150
    ret     nc                  ; |x| >= 2^23, infinity or NaN
    sub     #127
    jr      c, __truncf_zero    ; |x| < 1
    ld      h, a                ; H = number of integer bits in the mantissa
    ; The boundary between integer and fraction bits falls in one byte.
    cpl
    and     #7
    ld      l, a            ; L = number of fraction bits in that byte
    ld      a, #0xFF
    jr      z, __truncf_clear
__truncf_mask:
    add     a, a
    dec     l
    jr      nz, __truncf_mask
__truncf_clear:
    ld      l, a            ; L = the integer bits of that byte
    ld      a, h
    cp      #16
    jr      nc, __truncf_c
    ld      c, #0
    cp      #8
    jr      nc, __truncf_b
    ld      b, #0
    ld      a, l
    and     e
    ld      e, a
    ret
__truncf_b:
    ld      a, l
    and     b
    ld      b, a
    ret
__truncf_c:
    ld      a, l
    and     c
    ld      c, a
    ret
__truncf_zero:
    ld      a, d
    and     #0x80
    ld      d, a
    xor     a
    ld      e, a
    ld      b, a
    ld      c, a
    ret
