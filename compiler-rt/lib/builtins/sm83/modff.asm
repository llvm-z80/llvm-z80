; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
    .area   _CODE
    .globl  _modff
;===------------------------------------------------------------------------===;
;@ float modff(float x, float *iptr)
;@     modifies
;@         *iptr;
;@     ensures
;@         same(result, modff(x, &(float){0}));
;@         same(*iptr, truncf(x));
;@     tests
;@         samples 40000000;
;@         example x = -0.0f;
;@         example x = -INFINITY;
;@         example x = NAN;
;@         example x = 0x1p-149f;
;@         example x = -1.5f;
;@         example x = 0x1.000002p0f;
;@         example x = 0x1.fffffep22f;
;===------------------------------------------------------------------------===;
_modff:
    push    de
    push    bc
    ld      a, e
    rlca
    ld      a, d
    rla                         ; A = exponent
    cp      #150
    jr      nc, __modff_store   ; |x| >= 2^23, infinity or NaN
    cp      #127
    jr      nc, __modff_trunc
    ld      a, d                ; |x| < 1
    and     #0x80
    ld      d, a
    xor     a
    ld      e, a
    ld      b, a
    ld      c, a
    jr      __modff_store
__modff_trunc:
    call    _truncf
__modff_store:
    ldhl    sp, #6
    ld      a, (hl+)
    ld      h, (hl)
    ld      l, a                ; HL = iptr
    ld      a, c
    ld      (hl+), a
    ld      a, b
    ld      (hl+), a
    ld      a, e
    ld      (hl+), a
    ld      (hl), d
    pop     bc
    pop     de                  ; DEBC = x
    ld      a, e
    rlca
    ld      a, d
    rla                         ; A = exponent
    cp      #127
    jr      c, __modff_ret      ; |x| < 1 is all fraction
    cp      #150
    jr      c, __modff_frac
    inc     a
    jr      nz, __modff_zero    ; |x| >= 2^23 is an integer
    ld      a, e
    add     a, a
    or      b
    or      c
    jr      nz, __modff_ret     ; NaN
    jr      __modff_zero        ; infinity
__modff_frac:
    ; The bits x has and its integral part lacks are the fraction.
    push    af
    dec     hl
    ld      a, (hl)
    xor     e
    ld      e, a
    dec     hl
    ld      a, (hl)
    xor     b
    ld      b, a
    dec     hl
    ld      a, (hl)
    xor     c
    ld      c, a
    pop     af
    ld      h, a                ; H = exponent
    ld      a, e
    or      b
    or      c
    jr      z, __modff_zero     ; x is an integer
    ; Shift the fraction up to the hidden bit, lowering the exponent.
__modff_byte:
    ld      a, e
    or      a
    jr      nz, __modff_bit
    ld      e, b
    ld      b, c
    ld      c, a
    ld      a, h
    sub     #8
    ld      h, a
    jr      __modff_byte
__modff_bit:
    bit     7, e
    jr      nz, __modff_pack
    sla     c
    rl      b
    rl      e
    dec     h
    jr      __modff_bit
__modff_pack:
    ld      a, e
    add     a, a            ; drop the hidden bit
    srl     h
    rra
    ld      e, a
    ld      a, d
    and     #0x80
    or      h
    ld      d, a
    jr      __modff_ret
__modff_zero:
    ld      a, d
    and     #0x80
    ld      d, a
    xor     a
    ld      e, a
    ld      b, a
    ld      c, a
__modff_ret:
    pop     hl
    add     sp, #2
    jp      (hl)
