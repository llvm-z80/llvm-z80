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
    push    de
    push    bc
    add     sp, #-4
    ; SP+0 = count, SP+2 = s, SP+4 to SP+7 = a, SP+10 to SP+13 = b
    ld      a, e
    rlca
    ld      a, d
    rla
    inc     a
    jp      z, __fmodf_nan      ; a is infinite or NaN
    ldhl    sp, #13
    res     7, (hl)             ; b = |b|
    ld      a, (hl)
    dec     hl
    or      (hl)
    dec     hl
    or      (hl)
    dec     hl
    or      (hl)
    jp      z, __fmodf_nan      ; b = 0
    ldhl    sp, #12
    ld      a, (hl+)
    rlca
    ld      a, (hl)
    rla
    inc     a
    jr      nz, __fmodf_cmp
    dec     hl
    ld      a, (hl)
    add     a, a
    dec     hl
    or      (hl)
    dec     hl
    or      (hl)
    jp      nz, __fmodf_nan     ; b is NaN
__fmodf_cmp:
    ; Compare |a| with |b|.
    res     7, d
    ldhl    sp, #10
    ld      a, c
    sub     (hl)
    ld      c, a
    inc     hl
    ld      a, b
    sbc     a, (hl)
    ld      b, a
    inc     hl
    ld      a, e
    sbc     a, (hl)
    ld      e, a
    inc     hl
    ld      a, d
    sbc     a, (hl)
    jp      c, __fmodf_a        ; |a| < |b|
    or      c
    or      b
    or      e
    jp      z, __fmodf_zero     ; |a| = |b|
    ; Unpack b into SP+10 to SP+12 with the hidden bit at the top and its
    ; exponent into SP+13. A subnormal b takes exponent 1 and is shifted up
    ; to the hidden bit by s places.
    ldhl    sp, #12
    ld      a, (hl+)
    rlca
    ld      a, (hl-)
    rla                     ; A = exponent of b
    ld      c, #0
    set     7, (hl)
    or      a
    jr      nz, __fmodf_bdone
    res     7, (hl)
    inc     a
__fmodf_bnorm:
    bit     7, (hl)
    jr      nz, __fmodf_bdone
    dec     hl
    dec     hl
    sla     (hl)
    inc     hl
    rl      (hl)
    inc     hl
    rl      (hl)
    inc     c
    jr      __fmodf_bnorm
__fmodf_bdone:
    inc     hl
    ld      (hl), a
    ld      b, a            ; B = exponent of b
    ldhl    sp, #2
    ld      (hl), c
    ; count = exponent of a - exponent of b + s
    ldhl    sp, #6
    ld      a, (hl+)
    rlca
    ld      a, (hl)
    rla                     ; A = exponent of a
    or      a
    jr      nz, __fmodf_count
    inc     a
__fmodf_count:
    sub     b
    add     a, c
    ld      e, a
    ld      a, #0
    adc     a, a
    ldhl    sp, #1
    ld      (hl-), a
    ld      (hl), e
    ; Unpack a into D:E:B:C the same way, without shifting a subnormal.
    ldhl    sp, #4
    ld      c, (hl)
    inc     hl
    ld      b, (hl)
    inc     hl
    ld      a, (hl+)
    ld      e, a
    rlca
    ld      a, (hl)
    rla                     ; A = exponent of a
    set     7, e
    or      a
    jr      nz, __fmodf_adone
    res     7, e
__fmodf_adone:
    ld      d, #0
    ; Long division keeping only the remainder: subtract b where it fits,
    ; and repeat after a shift count more times.
__fmodf_loop:
    ld      a, d
    or      a
    jr      nz, __fmodf_sub     ; above 2^24, so above b
    ldhl    sp, #12
    ld      a, e
    cp      (hl)
    jr      nz, __fmodf_fit
    dec     hl
    ld      a, b
    cp      (hl)
    jr      nz, __fmodf_fit
    dec     hl
    ld      a, c
    cp      (hl)
__fmodf_fit:
    jr      c, __fmodf_next     ; below b
__fmodf_sub:
    ldhl    sp, #10
    ld      a, c
    sub     (hl)
    ld      c, a
    inc     hl
    ld      a, b
    sbc     a, (hl)
    ld      b, a
    inc     hl
    ld      a, e
    sbc     a, (hl)
    ld      e, a
    ld      d, #0
    or      b
    or      c
    jp      z, __fmodf_zero
__fmodf_next:
    ldhl    sp, #0
    ld      a, (hl+)
    or      (hl)
    jr      z, __fmodf_done
    dec     hl
    ld      a, (hl)
    sub     #1
    ld      (hl+), a
    jr      nc, __fmodf_shift
    dec     (hl)
__fmodf_shift:
    sla     c
    rl      b
    rl      e
    rl      d
    jr      __fmodf_loop
__fmodf_done:
    ldhl    sp, #2
    ld      a, (hl)
    or      a
    jr      nz, __fmodf_small
    ldhl    sp, #13
    ld      h, (hl)             ; H = exponent of b
    ; Shift the remainder up to the hidden bit while the exponent stays above 1.
__fmodf_norm:
    bit     7, e
    jr      nz, __fmodf_pack
    ld      a, h
    dec     a
    jr      z, __fmodf_pack
    ld      h, a
    sla     c
    rl      b
    rl      e
    jr      __fmodf_norm
__fmodf_small:
    ; A subnormal b leaves a subnormal remainder: shift it back down by s.
    ld      h, a
__fmodf_shr:
    srl     e
    rr      b
    rr      c
    dec     h
    jr      nz, __fmodf_shr
__fmodf_pack:
    bit     7, e
    jr      nz, __fmodf_normal
    ld      h, #0           ; subnormal
__fmodf_normal:
    ld      a, e
    add     a, a            ; drop the hidden bit
    srl     h
    rra
    ld      e, a
    ld      d, h
    ldhl    sp, #7
    ld      a, (hl)
    and     #0x80
    or      d
    ld      d, a
    jr      __fmodf_ret
__fmodf_a:
    ldhl    sp, #4
    ld      c, (hl)
    inc     hl
    ld      b, (hl)
    inc     hl
    ld      e, (hl)
    inc     hl
    ld      d, (hl)
    jr      __fmodf_ret
__fmodf_nan:
    ld      de, #0x7FC0
    ld      bc, #0
    jr      __fmodf_ret
__fmodf_zero:
    ldhl    sp, #7
    ld      a, (hl)
    and     #0x80
    ld      d, a
    xor     a
    ld      e, a
    ld      b, a
    ld      c, a
__fmodf_ret:
    add     sp, #8
    pop     hl
    add     sp, #4
    jp      (hl)
