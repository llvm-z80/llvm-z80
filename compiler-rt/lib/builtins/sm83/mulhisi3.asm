; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
    .area   _CODE
    .globl  ___mulhisi3
;===------------------------------------------------------------------------===;
;@ int32_t __mulhisi3(int16_t a, int16_t b)
;@     ensures
;@         result == (int32_t)a * b;
;@     tests
;@         exhaustive;
;===------------------------------------------------------------------------===;
___mulhisi3:
    bit     7, d
    jr      nz, ___mulhisi3_a_neg
    bit     7, b
    jp      z, ___umulhisi3     ; a, b >= 0: no correction
    ld      h, d                ; b < 0 <= a: HL = a
    ld      l, e
    jr      ___mulhisi3_fix
___mulhisi3_a_neg:
    ld      l, c                ; a < 0: HL = b
    ld      h, b
    bit     7, b
    jr      z, ___mulhisi3_fix
    add     hl, de              ; b < 0: HL += a
___mulhisi3_fix:
    push    hl
    call    ___umulhisi3        ; DE:BC = a * b, unsigned
    pop     hl
    ld      a, e                ; high half -= HL
    sub     l
    ld      e, a
    ld      a, d
    sbc     a, h
    ld      d, a
    ret
