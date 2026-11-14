; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
    .area   _CODE
    .globl  ___umulhisi3
    .globl  ___umulhi3
;===------------------------------------------------------------------------===;
;@ uint32_t __umulhisi3(uint16_t a, uint16_t b)
;@     ensures
;@         result == (uint32_t)a * b;
;@     tests
;@         exhaustive;
;===------------------------------------------------------------------------===;
___umulhisi3:
    call    ___umulhi3      ; DE = high half, HL = low half
    ex      de, hl
    ret
