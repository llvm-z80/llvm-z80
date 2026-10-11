; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
    .area   _CODE
    .globl  ___z80_memmove_builtin
;===------------------------------------------------------------------------===;
;@ void __z80_memmove_builtin(uint8_t *dst __reg(DE), const uint8_t *src __reg(BC), uint16_t n __reg(HL))
;@     modifies
;@         dst[0 .. n];
;@     ensures
;@         forall(i, 0, n, dst[i] == old(src[i]));
;@     tests
;@         samples 1000000;
;@         n in 0 .. 600;
;===------------------------------------------------------------------------===;
___z80_memmove_builtin:
    ld      a, h
    or      l
    ret     z                           ; n = 0
    ld      a, e
    sub     c
    ld      a, d
    sbc     a, b
    jr      c, ___z80_memmove_fwd       ; dst < src copies upward
    ld      a, e
    sub     c
    jr      nz, ___z80_memmove_back
    ld      a, d
    sub     b
    ret     z                           ; dst = src
___z80_memmove_back:
    ; dst > src copies downward from the last byte of each region.
    push    hl
    ld      h, b
    ld      l, c            ; HL = src
    pop     bc              ; BC = n
    add     hl, bc
    dec     hl              ; HL = src + n - 1
    push    hl
    ld      h, d
    ld      l, e
    add     hl, bc
    dec     hl              ; HL = dst + n - 1
    pop     de              ; DE = src + n - 1
    ; Count C down to zero B times, with B one higher when C starts nonzero.
    inc     c
    dec     c
    jr      z, ___z80_memmove_back_loop
    inc     b
___z80_memmove_back_loop:
    ld      a, (de)
    dec     de
    ld      (hl-), a
    dec     c
    jr      nz, ___z80_memmove_back_loop
    dec     b
    jr      nz, ___z80_memmove_back_loop
    ret
___z80_memmove_fwd:
    push    bc
    ld      b, h
    ld      c, l            ; BC = n
    pop     hl              ; HL = src
    inc     c
    dec     c
    jr      z, ___z80_memmove_fwd_loop
    inc     b
___z80_memmove_fwd_loop:
    ld      a, (hl+)
    ld      (de), a
    inc     de
    dec     c
    jr      nz, ___z80_memmove_fwd_loop
    dec     b
    jr      nz, ___z80_memmove_fwd_loop
    ret
