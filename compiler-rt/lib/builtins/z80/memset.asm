; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
    .area   _CODE
    .globl  _memset
;===------------------------------------------------------------------------===;
;@ uint8_t *memset(uint8_t *dst, int16_t c, uint16_t n)
;@     modifies
;@         dst[0 .. n];
;@     ensures
;@         result == dst;
;@         forall(i, 0, n, dst[i] == (uint8_t)c);
;@     tests
;@         n in 0 .. 600;
;===------------------------------------------------------------------------===;
_memset:
    pop     iy              ; IY = return address
    pop     bc              ; BC = n
    push    hl
    call    ___z80_memset_builtin
    pop     de              ; DE = dst
    jp      (iy)
