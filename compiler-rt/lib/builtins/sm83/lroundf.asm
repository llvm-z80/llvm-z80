; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
    .area   _CODE
    .globl  _lroundf
;===------------------------------------------------------------------------===;
;@ int32_t lroundf(float x)
;@     requires
;@         x >= -0x1p31f && x < 0x1p31f;
;@     ensures
;@         result == llroundf(x);
;@     tests
;@         samples 1500000000;
;===------------------------------------------------------------------------===;
_lroundf:
    ld      l, d                ; L = sign
    ld      a, e
    rlca
    ld      a, d
    rla                         ; A = exponent
    cp      #158
    jr      nc, __lroundf_big   ; |x| >= 2^31, infinity or NaN
    sub     #126
    jr      c, __lroundf_zero   ; |x| < 0.5
    set     7, e                ; E:B:C = significand
    ld      d, #0
    sub     #24
    jr      nc, __lroundf_left  ; |x| >= 2^23
    cpl                         ; A = number of fraction bits - 1
    ; Move whole fraction bytes out of C.
__lroundf_byte:
    cp      #8
    jr      c, __lroundf_bit
    ld      c, b
    ld      b, e
    ld      e, d
    sub     #8
    jr      __lroundf_byte
__lroundf_bit:
    ld      h, a
    inc     h
__lroundf_shift:
    srl     e
    rr      b
    rr      c
    dec     h
    jr      nz, __lroundf_shift ; carry = the one-half bit
    jr      nc, __lroundf_sign
    inc     c
    jr      nz, __lroundf_sign
    inc     b
    jr      nz, __lroundf_sign
    inc     e
    jr      __lroundf_sign
__lroundf_left:
    jr      z, __lroundf_sign
    ld      h, a
__lroundf_shl:
    sla     c
    rl      b
    rl      e
    rl      d
    dec     h
    jr      nz, __lroundf_shl
__lroundf_sign:
    bit     7, l
    ret     z
    xor     a
    sub     c
    ld      c, a
    ld      a, #0
    sbc     a, b
    ld      b, a
    ld      a, #0
    sbc     a, e
    ld      e, a
    ld      a, #0
    sbc     a, d
    ld      d, a
    ret
__lroundf_big:
    ld      de, #0x8000
    ld      bc, #0
    ret
__lroundf_zero:
    ld      de, #0
    ld      b, d
    ld      c, e
    ret
