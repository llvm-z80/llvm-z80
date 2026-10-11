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
    push    hl
    push    de
    ld      a, l
    rlca
    ld      a, h
    rla                         ; A = exponent
    cp      #150
    jr      nc, __modff_store   ; |x| >= 2^23, infinity or NaN
    cp      #127
    jr      nc, __modff_trunc
    ld      a, h                ; |x| < 1
    and     #0x80
    ld      h, a
    xor     a
    ld      l, a
    ld      d, a
    ld      e, a
    jr      __modff_store
__modff_trunc:
    call    _truncf
__modff_store:
    push    hl
    ld      hl, #8
    add     hl, sp
    ld      c, (hl)
    inc     hl
    ld      b, (hl)             ; BC = iptr
    pop     hl
    ld      a, e
    ld      (bc), a
    inc     bc
    ld      a, d
    ld      (bc), a
    inc     bc
    ld      a, l
    ld      (bc), a
    inc     bc
    ld      a, h
    ld      (bc), a
    pop     de
    pop     hl                  ; HLDE = x
    ld      a, l
    rlca
    ld      a, h
    rla                         ; A = exponent
    cp      #127
    jr      c, __modff_ret      ; |x| < 1 is all fraction
    cp      #150
    jr      c, __modff_frac
    inc     a
    jr      nz, __modff_zero    ; |x| >= 2^23 is an integer
    ld      a, l
    add     a, a
    or      d
    or      e
    jr      nz, __modff_ret     ; NaN
    jr      __modff_zero        ; infinity
__modff_frac:
    ; The bits x has and its integral part lacks are the fraction.
    push    af
    dec     bc
    ld      a, (bc)
    xor     l
    ld      l, a
    dec     bc
    ld      a, (bc)
    xor     d
    ld      d, a
    dec     bc
    ld      a, (bc)
    xor     e
    ld      e, a
    pop     af
    ld      b, a                ; B = exponent
    ld      a, l
    or      d
    or      e
    jr      z, __modff_zero     ; x is an integer
    ; Shift the fraction up to the hidden bit, lowering the exponent.
__modff_byte:
    ld      a, l
    or      a
    jr      nz, __modff_bit
    ld      l, d
    ld      d, e
    ld      e, a
    ld      a, b
    sub     #8
    ld      b, a
    jr      __modff_byte
__modff_bit:
    bit     7, l
    jr      nz, __modff_pack
    sla     e
    rl      d
    rl      l
    dec     b
    jr      __modff_bit
__modff_pack:
    ld      a, l
    add     a, a            ; drop the hidden bit
    srl     b
    rra
    ld      l, a
    ld      a, h
    and     #0x80
    or      b
    ld      h, a
    jr      __modff_ret
__modff_zero:
    ld      a, h
    and     #0x80
    ld      h, a
    xor     a
    ld      l, a
    ld      d, a
    ld      e, a
__modff_ret:
    pop     bc
    inc     sp
    inc     sp
    push    bc
    ret
