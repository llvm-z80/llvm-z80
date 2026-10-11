; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
    .area   _CODE
    .globl  _lrintf
;===------------------------------------------------------------------------===;
;@ int32_t lrintf(float x)
;@     requires
;@         x >= -0x1p31f && x < 0x1p31f;
;@     ensures
;@         result == llrintf(x);
;@     tests
;@         samples 1500000000;
;===------------------------------------------------------------------------===;
_lrintf:
    ld      l, d                ; L = sign
    ld      a, e
    rlca
    ld      a, d
    rla                         ; A = exponent
    cp      #158
    jr      nc, __lrintf_big    ; |x| >= 2^31, infinity or NaN
    sub     #126
    jr      c, __lrintf_zero    ; |x| < 0.5
    set     7, e                ; E:B:C = significand
    ld      d, #0
    sub     #24
    jr      nc, __lrintf_left   ; |x| >= 2^23
    cpl                         ; A = number of fraction bits - 1
    ; Move whole fraction bytes out of C, keeping whether they were zero in D.
__lrintf_byte:
    cp      #8
    jr      c, __lrintf_bit
    ld      h, a
    ld      a, c
    or      d
    ld      d, a
    ld      c, b
    ld      b, e
    ld      e, #0
    ld      a, h
    sub     #8
    jr      __lrintf_byte
__lrintf_bit:
    or      a
    jr      z, __lrintf_half
    ld      h, a
__lrintf_shift:
    srl     e
    rr      b
    rr      c
    jr      nc, __lrintf_next
    ld      d, #1
__lrintf_next:
    dec     h
    jr      nz, __lrintf_shift
__lrintf_half:
    srl     e
    rr      b
    rr      c                   ; carry = the one-half bit
    jr      nc, __lrintf_done   ; below one half
    ld      a, d
    or      a
    jr      nz, __lrintf_up     ; above one half
    bit     0, c
    jr      z, __lrintf_done    ; a tie with an even integer
__lrintf_up:
    inc     c
    jr      nz, __lrintf_done
    inc     b
    jr      nz, __lrintf_done
    inc     e
__lrintf_done:
    ld      d, #0
    jr      __lrintf_sign
__lrintf_left:
    jr      z, __lrintf_sign
    ld      h, a
__lrintf_shl:
    sla     c
    rl      b
    rl      e
    rl      d
    dec     h
    jr      nz, __lrintf_shl
__lrintf_sign:
    bit     7, l
    ret     z
    xor     a
    sub     c
    ld      c, a
    ld      a, #0
    sbc     a, b
    ld      b, a
    ld      a, #0
    sbc     a, e
    ld      e, a
    ld      a, #0
    sbc     a, d
    ld      d, a
    ret
__lrintf_big:
    ld      de, #0x8000
    ld      bc, #0
    ret
__lrintf_zero:
    ld      de, #0
    ld      b, d
    ld      c, e
    ret
