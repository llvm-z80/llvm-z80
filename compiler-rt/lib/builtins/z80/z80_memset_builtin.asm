; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
    .area   _CODE
    .globl  ___z80_memset_builtin
;===------------------------------------------------------------------------===;
;@ void __z80_memset_builtin(uint8_t *dst __reg(HL), uint16_t c __reg(DE), uint16_t n __reg(BC))
;@     modifies
;@         dst[0 .. n];
;@     ensures
;@         forall(i, 0, n, dst[i] == (uint8_t)c);
;@         IX == old(IX);
;@     tests
;@         samples 1000000;
;@         n in 0 .. 600;
;===------------------------------------------------------------------------===;
___z80_memset_builtin:
    ld      a, b
    or      c
    ret     z               ; n = 0
    ld      (hl), e         ; the first byte
    dec     bc
    ld      a, b
    or      c
    ret     z               ; n = 1
    ld      d, h
    ld      e, l
    inc     de
    ldir                    ; copy each byte into the next
    ret
