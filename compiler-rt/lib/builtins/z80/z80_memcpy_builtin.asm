; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
    .area   _CODE
    .globl  ___z80_memcpy_builtin
;===------------------------------------------------------------------------===;
;@ void __z80_memcpy_builtin(uint8_t *dst __reg(HL), const uint8_t *src __reg(DE), uint16_t n __reg(BC))
;@     requires
;@         dst + n <= src || src + n <= dst;
;@     modifies
;@         dst[0 .. n];
;@     ensures
;@         forall(i, 0, n, dst[i] == old(src[i]));
;@         IX == old(IX);
;@     tests
;@         samples 1000000;
;@         n in 0 .. 600;
;===------------------------------------------------------------------------===;
___z80_memcpy_builtin:
    ex      de, hl          ; HL = src, DE = dst
    ld      a, b
    or      c
    ret     z               ; LDIR would take n = 0 as 65536
    ldir
    ret
