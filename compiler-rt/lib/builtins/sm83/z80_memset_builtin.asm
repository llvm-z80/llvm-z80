; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
    .area   _CODE
    .globl  ___z80_memset_builtin
;===------------------------------------------------------------------------===;
;@ void __z80_memset_builtin(uint8_t *dst __reg(DE), uint16_t c __reg(BC), uint16_t n __reg(HL))
;@     modifies
;@         dst[0 .. n];
;@     ensures
;@         forall(i, 0, n, dst[i] == (uint8_t)c);
;@     tests
;@         samples 1000000;
;@         n in 0 .. 600;
;===------------------------------------------------------------------------===;
___z80_memset_builtin:
    ld      a, h
    or      l
    ret     z               ; n = 0
    ld      a, c            ; A = the fill byte, held for the whole loop
    ld      b, h
    ld      c, l            ; BC = n
    ld      h, d
    ld      l, e            ; HL = dst, the pointer SM83 can step
    ; Count C down to zero B times, with B one higher when C starts nonzero.
    inc     c
    dec     c
    jr      z, ___z80_memset_loop
    inc     b
___z80_memset_loop:
    ld      (hl+), a
    dec     c
    jr      nz, ___z80_memset_loop
    dec     b
    jr      nz, ___z80_memset_loop
    ret
