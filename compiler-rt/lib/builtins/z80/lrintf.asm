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
    ld      c, h                ; C = sign
    ld      a, l
    rlca
    ld      a, h
    rla                         ; A = exponent
    cp      #158
    jr      nc, __lrintf_big    ; |x| >= 2^31, infinity or NaN
    sub     #126
    jr      c, __lrintf_zero    ; |x| < 0.5
    set     7, l                ; L:D:E = significand
    ld      h, #0
    sub     #24
    jr      nc, __lrintf_left   ; |x| >= 2^23
    cpl                         ; A = number of fraction bits - 1
    ; Move whole fraction bytes out of E, keeping whether they were zero in H.
__lrintf_byte:
    cp      #8
    jr      c, __lrintf_bit
    ld      b, a
    ld      a, e
    or      h
    ld      h, a
    ld      e, d
    ld      d, l
    ld      l, #0
    ld      a, b
    sub     #8
    jr      __lrintf_byte
__lrintf_bit:
    or      a
    jr      z, __lrintf_half
    ld      b, a
__lrintf_shift:
    srl     l
    rr      d
    rr      e
    jr      nc, __lrintf_next
    ld      h, #1
__lrintf_next:
    djnz    __lrintf_shift
__lrintf_half:
    srl     l
    rr      d
    rr      e                   ; carry = the one-half bit
    jr      nc, __lrintf_done   ; below one half
    ld      a, h
    or      a
    jr      nz, __lrintf_up     ; above one half
    bit     0, e
    jr      z, __lrintf_done    ; a tie with an even integer
__lrintf_up:
    inc     e
    jr      nz, __lrintf_done
    inc     d
    jr      nz, __lrintf_done
    inc     l
__lrintf_done:
    ld      h, #0
    jr      __lrintf_sign
__lrintf_left:
    jr      z, __lrintf_sign
    ld      b, a
__lrintf_shl:
    sla     e
    rl      d
    rl      l
    rl      h
    djnz    __lrintf_shl
__lrintf_sign:
    bit     7, c
    ret     z
    xor     a
    sub     e
    ld      e, a
    ld      a, #0
    sbc     a, d
    ld      d, a
    ld      a, #0
    sbc     a, l
    ld      l, a
    ld      a, #0
    sbc     a, h
    ld      h, a
    ret
__lrintf_big:
    ld      hl, #0x8000
    ld      de, #0
    ret
__lrintf_zero:
    ld      hl, #0
    ld      d, h
    ld      e, l
    ret
