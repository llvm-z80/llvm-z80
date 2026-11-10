; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
    .area   _CODE
    .globl  _memcpy
    .globl  ___memcpy
;===------------------------------------------------------------------------===;
;@ uint8_t *memcpy(uint8_t *dst, const uint8_t *src, uint16_t n)
;@     requires
;@         dst + n <= src || src + n <= dst;
;@     modifies
;@         dst[0 .. n];
;@     ensures
;@         result == dst;
;@         forall(i, 0, n, dst[i] == old(src[i]));
;@     tests
;@         samples 1000000;
;@         n in 0 .. 600;
;===------------------------------------------------------------------------===;
_memcpy:
___memcpy:
    pop     iy                  ; IY = return address
    pop     bc                  ; BC = n
    push    hl
    ex      de, hl              ; HL = src, DE = dst
    ld      a, b
    or      c
    jr      z, __memcpy_done    ; LDIR would take n = 0 as 65536
    ldir
__memcpy_done:
    pop     de                  ; DE = dst
    jp      (iy)
