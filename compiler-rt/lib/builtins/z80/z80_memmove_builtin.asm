; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
    .area   _CODE
    .globl  ___z80_memmove_builtin
;===------------------------------------------------------------------------===;
;@ void __z80_memmove_builtin(uint8_t *dst __reg(HL), const uint8_t *src __reg(DE), uint16_t n __reg(BC))
;@     modifies
;@         dst[0 .. n];
;@     ensures
;@         forall(i, 0, n, dst[i] == old(src[i]));
;@         IX == old(IX);
;@     tests
;@         samples 1000000;
;@         n in 0 .. 600;
;===------------------------------------------------------------------------===;
___z80_memmove_builtin:
    ld      a, b
    or      c
    ret     z                           ; LDIR and LDDR take n = 0 as 65536
    push    hl
    sbc     hl, de                      ; OR left carry clear
    pop     hl
    ret     z                           ; dst = src
    jr      c, ___z80_memmove_fwd       ; dst < src copies upward
    ; dst > src copies downward from the last byte of each region.
    add     hl, bc
    dec     hl              ; HL = dst + n - 1
    ex      de, hl
    add     hl, bc
    dec     hl              ; HL = src + n - 1
    lddr
    ret
___z80_memmove_fwd:
    ex      de, hl          ; HL = src, DE = dst
    ldir
    ret
