; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
    .area   _CODE
    .globl  _memmove
;===------------------------------------------------------------------------===;
;@ uint8_t *memmove(uint8_t *dst, const uint8_t *src, uint16_t n)
;@     modifies
;@         dst[0 .. n];
;@     ensures
;@         result == dst;
;@         forall(i, 0, n, dst[i] == old(src[i]));
;@     tests
;@         samples 1000000;
;@         n in 0 .. 600;
;===------------------------------------------------------------------------===;
_memmove:
    pop     iy              ; IY = return address
    pop     bc              ; BC = n
    push    hl
    call    ___z80_memmove_builtin
    pop     de              ; DE = dst
    jp      (iy)
