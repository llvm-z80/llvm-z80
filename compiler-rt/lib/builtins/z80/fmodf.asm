; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
    .area   _CODE
    .globl  _fmodf
;===------------------------------------------------------------------------===;
;@ float fmodf(float a, float b)
;@     ensures
;@         same(result, fmodf(a, b));
;@     tests
;@         example a = 0x1.fffffep127f, b = 0x1p-149f;
;@         example a = 0x1.fffffep127f, b = 0x1.fffffep-127f;
;@         example a = 0x1.8p-140f, b = 0x1p-148f;
;@         example a = -5.5f, b = 2.0f;
;@         example a = 6.0f, b = -3.0f;
;===------------------------------------------------------------------------===;
_fmodf:
    push    ix
    ld      ix, #0
    add     ix, sp
    push    hl
    push    de
    dec     sp
    ; 4(ix) to 7(ix) = b, -4(ix) to -1(ix) = a, -5(ix) = s
    ld      a, 7(ix)
    add     a, a
    or      6(ix)
    or      5(ix)
    or      4(ix)
    jp      z, __fmodf_nan      ; b = 0
    ld      a, l
    rlca
    ld      a, h
    rla
    inc     a
    jp      z, __fmodf_nan      ; a is infinite or NaN
    ld      a, 6(ix)
    rlca
    ld      a, 7(ix)
    rla
    inc     a
    jr      nz, __fmodf_cmp
    ld      a, 6(ix)
    add     a, a
    or      5(ix)
    or      4(ix)
    jp      nz, __fmodf_nan     ; b is NaN
__fmodf_cmp:
    ; Compare |a| with |b|.
    ld      a, 7(ix)
    and     #0x7F
    ld      b, a
    res     7, h
    ld      a, e
    sub     4(ix)
    ld      c, a
    ld      a, d
    sbc     a, 5(ix)
    ld      e, a
    ld      a, l
    sbc     a, 6(ix)
    ld      d, a
    ld      a, h
    sbc     a, b
    jp      c, __fmodf_a        ; |a| < |b|
    or      c
    or      e
    or      d
    jp      z, __fmodf_zero     ; |a| = |b|
    ; Unpack b into 4(ix) to 6(ix) with the hidden bit at the top and its
    ; exponent into 7(ix). A subnormal b takes exponent 1 and is shifted up
    ; to the hidden bit by s places.
    ld      a, 6(ix)
    rlca
    ld      a, 7(ix)
    rla                     ; A = exponent of b
    ld      c, #0
    set     7, 6(ix)
    or      a
    jr      nz, __fmodf_bdone
    res     7, 6(ix)
    inc     a
__fmodf_bnorm:
    bit     7, 6(ix)
    jr      nz, __fmodf_bdone
    sla     4(ix)
    rl      5(ix)
    rl      6(ix)
    inc     c
    jr      __fmodf_bnorm
__fmodf_bdone:
    ld      7(ix), a
    ld      -5(ix), c
    ; Unpack a into H:L:D:E the same way, without shifting a subnormal.
    ld      e, -4(ix)
    ld      d, -3(ix)
    ld      l, -2(ix)
    ld      a, l
    rlca
    ld      a, -1(ix)
    rla                     ; A = exponent of a
    set     7, l
    or      a
    jr      nz, __fmodf_adone
    res     7, l
    inc     a
__fmodf_adone:
    ld      h, #0
    ; BC = exponent of a - exponent of b + s
    sub     7(ix)
    add     a, c
    ld      c, a
    ld      a, #0
    adc     a, a
    ld      b, a
    ; Long division keeping only the remainder: subtract b where it fits,
    ; and repeat after a shift BC more times.
__fmodf_loop:
    ld      a, h
    or      a
    jr      nz, __fmodf_sub     ; above 2^24, so above b
    ld      a, l
    cp      6(ix)
    jr      nz, __fmodf_fit
    ld      a, d
    cp      5(ix)
    jr      nz, __fmodf_fit
    ld      a, e
    cp      4(ix)
__fmodf_fit:
    jr      c, __fmodf_next     ; below b
__fmodf_sub:
    ld      a, e
    sub     4(ix)
    ld      e, a
    ld      a, d
    sbc     a, 5(ix)
    ld      d, a
    ld      a, l
    sbc     a, 6(ix)
    ld      l, a
    ld      h, #0
    or      d
    or      e
    jp      z, __fmodf_zero
__fmodf_next:
    ld      a, b
    or      c
    jr      z, __fmodf_done
    dec     bc
    sla     e
    rl      d
    rl      l
    rl      h
    jr      __fmodf_loop
__fmodf_done:
    ld      a, -5(ix)
    or      a
    jr      nz, __fmodf_small
    ld      b, 7(ix)
    ; Shift the remainder up to the hidden bit while the exponent stays above 1.
__fmodf_norm:
    bit     7, l
    jr      nz, __fmodf_pack
    ld      a, b
    dec     a
    jr      z, __fmodf_pack
    ld      b, a
    sla     e
    rl      d
    rl      l
    jr      __fmodf_norm
__fmodf_small:
    ; A subnormal b leaves a subnormal remainder: shift it back down by s.
    ld      b, a
__fmodf_shr:
    srl     l
    rr      d
    rr      e
    djnz    __fmodf_shr
__fmodf_pack:
    bit     7, l
    jr      nz, __fmodf_normal
    ld      b, #0           ; subnormal
__fmodf_normal:
    ld      a, l
    add     a, a            ; drop the hidden bit
    srl     b
    rra
    ld      l, a
    ld      a, -1(ix)
    and     #0x80
    or      b
    ld      h, a
    jr      __fmodf_ret
__fmodf_a:
    ld      e, -4(ix)
    ld      d, -3(ix)
    ld      l, -2(ix)
    ld      h, -1(ix)
    jr      __fmodf_ret
__fmodf_nan:
    ld      hl, #0x7FC0
    ld      de, #0
    jr      __fmodf_ret
__fmodf_zero:
    ld      a, -1(ix)
    and     #0x80
    ld      h, a
    xor     a
    ld      l, a
    ld      d, a
    ld      e, a
__fmodf_ret:
    ld      sp, ix
    pop     ix
    pop     bc
    inc     sp
    inc     sp
    inc     sp
    inc     sp
    push    bc
    ret
