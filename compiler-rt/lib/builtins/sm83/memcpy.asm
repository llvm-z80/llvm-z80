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
    push    de              ; keep dst to return
    ldhl    sp, #4          ; skip dst and the return address
    ld      a, (hl+)
    ld      h, (hl)
    ld      l, a            ; HL = n
    call    ___z80_memcpy_builtin
    pop     bc              ; BC = dst
    pop     hl              ; HL = return address
    add     sp, #2          ; drop n
    jp      (hl)
