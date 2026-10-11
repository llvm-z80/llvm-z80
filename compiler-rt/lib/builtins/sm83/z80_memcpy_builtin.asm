; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
    .area   _CODE
    .globl  ___z80_memcpy_builtin
;===------------------------------------------------------------------------===;
;@ void __z80_memcpy_builtin(uint8_t *dst __reg(DE), const uint8_t *src __reg(BC), uint16_t n __reg(HL))
;@     requires
;@         dst + n <= src || src + n <= dst;
;@     modifies
;@         dst[0 .. n];
;@     ensures
;@         forall(i, 0, n, dst[i] == old(src[i]));
;@     tests
;@         samples 1000000;
;@         n in 0 .. 600;
;===------------------------------------------------------------------------===;
___z80_memcpy_builtin:
    ld      a, h
    or      l
    ret     z               ; n = 0
    push    bc
    ld      b, h
    ld      c, l            ; BC = n
    pop     hl              ; HL = src, the pointer SM83 can step
    ; Count C down to zero B times, with B one higher when C starts nonzero.
    inc     c
    dec     c
    jr      z, ___z80_memcpy_loop
    inc     b
___z80_memcpy_loop:
    ld      a, (hl+)
    ld      (de), a
    inc     de
    dec     c
    jr      nz, ___z80_memcpy_loop
    dec     b
    jr      nz, ___z80_memcpy_loop
    ret
