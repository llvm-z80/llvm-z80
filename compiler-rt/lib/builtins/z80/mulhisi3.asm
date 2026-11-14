; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
    .area   _CODE
    .globl  ___mulhisi3
    .globl  ___umulhi3
;===------------------------------------------------------------------------===;
;@ int32_t __mulhisi3(int16_t a, int16_t b)
;@     ensures
;@         result == (int32_t)a * b;
;@     tests
;@         exhaustive;
;===------------------------------------------------------------------------===;
___mulhisi3:
    push    de
    call    ___umulhi3      ; DE:HL = a * b, unsigned, BC = a
    ex      (sp), hl        ; HL = b, low half on the stack
    ex      de, hl          ; HL = high half, DE = b
    bit     7, b
    jr      z, ___mulhisi3_a_pos
    or      a
    sbc     hl, de          ; a < 0: high half -= b
___mulhisi3_a_pos:
    bit     7, d
    jr      z, ___mulhisi3_b_pos
    or      a
    sbc     hl, bc          ; b < 0: high half -= a
___mulhisi3_b_pos:
    pop     de              ; DE = low half
    ret
