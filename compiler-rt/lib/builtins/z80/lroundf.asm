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
    ld      c, h                ; C = sign
    ld      a, l
    rlca
    ld      a, h
    rla                         ; A = exponent
    cp      #158
    jr      nc, __lroundf_big   ; |x| >= 2^31, infinity or NaN
    sub     #126
    jr      c, __lroundf_zero   ; |x| < 0.5
    set     7, l                ; L:D:E = significand
    ld      h, #0
    sub     #24
    jr      nc, __lroundf_left  ; |x| >= 2^23
    cpl                         ; A = number of fraction bits - 1
    ; Move whole fraction bytes out of E.
__lroundf_byte:
    cp      #8
    jr      c, __lroundf_bit
    ld      e, d
    ld      d, l
    ld      l, h
    sub     #8
    jr      __lroundf_byte
__lroundf_bit:
    ld      b, a
    inc     b
__lroundf_shift:
    srl     l
    rr      d
    rr      e
    djnz    __lroundf_shift     ; carry = the one-half bit
    jr      nc, __lroundf_sign
    inc     e
    jr      nz, __lroundf_sign
    inc     d
    jr      nz, __lroundf_sign
    inc     l
    jr      __lroundf_sign
__lroundf_left:
    jr      z, __lroundf_sign
    ld      b, a
__lroundf_shl:
    sla     e
    rl      d
    rl      l
    rl      h
    djnz    __lroundf_shl
__lroundf_sign:
    bit     7, c
    ret     z
    xor     a
    sub     e
    ld      e, a
    ld      a, #0
    sbc     a, d
    ld      d, a
    ld      a, #0
    sbc     a, l
    ld      l, a
    ld      a, #0
    sbc     a, h
    ld      h, a
    ret
__lroundf_big:
    ld      hl, #0x8000
    ld      de, #0
    ret
__lroundf_zero:
    ld      hl, #0
    ld      d, h
    ld      e, l
    ret
