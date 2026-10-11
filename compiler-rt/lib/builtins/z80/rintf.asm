; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
    .area   _CODE
    .globl  _rintf
    .globl  _nearbyintf
    .globl  _roundevenf
;===------------------------------------------------------------------------===;
;@ float rintf(float x)
;@     ensures
;@         same(result, rintf(x));
;@     tests
;@         samples 1500000000;
;===------------------------------------------------------------------------===;
_rintf:
_nearbyintf:
_roundevenf:
    ld      a, l
    rlca
    ld      a, h
    rla                         ; A = exponent
    cp      #150
    ret     nc                  ; |x| >= 2^23, infinity or NaN
    sub     #126
    jr      c, __rintf_zero     ; |x| < 0.5
    jr      z, __rintf_half     ; 0.5 <= |x| < 1
    cpl
    add     a, #24              ; A = number of fraction bits - 1
    ; Move whole fraction bytes out of E, keeping whether they were zero.
    ld      b, #0
__rintf_byte:
    cp      #8
    jr      c, __rintf_bit
    ld      c, a
    ld      a, e
    or      b
    ld      b, a
    ld      e, d
    ld      d, l
    ld      l, h
    ld      h, #0
    ld      a, c
    sub     #8
    jr      __rintf_byte
__rintf_bit:
    ld      c, #1
    or      a
    jr      z, __rintf_round
__rintf_shift:
    sla     c
    dec     a
    jr      nz, __rintf_shift
__rintf_round:
    ; C = the one-half bit of E, B = the moved-out bytes ORed together.
    ld      a, e
    and     c
    jr      z, __rintf_down     ; below one half
    ld      a, c
    dec     a
    and     e
    or      b
    jr      nz, __rintf_up      ; above one half
    ld      a, c
    add     a, a                ; the lowest integer bit
    jr      nc, __rintf_tie
    bit     0, d
    jr      z, __rintf_down
    jr      __rintf_up
__rintf_tie:
    and     e
    jr      z, __rintf_down     ; a tie with an even integer part
__rintf_up:
    ld      a, c
    add     a, a
    dec     a
    or      e
    inc     a                   ; clear the fraction and add one
    ld      e, a
    jr      nz, __rintf_back
    inc     d
    jr      nz, __rintf_back
    inc     l
    jr      nz, __rintf_back
    inc     h
    jr      __rintf_back
__rintf_down:
    ld      a, c
    add     a, a
    dec     a
    cpl
    and     e
    ld      e, a
__rintf_back:
    ; Move the bytes back, filling the fraction bytes with zero.
    ld      a, h
    or      a
    ret     nz
    ld      h, l
    ld      l, d
    ld      d, e
    ld      e, a
    jr      __rintf_back
__rintf_half:
    ld      a, l
    and     #0x7F
    or      d
    or      e
    jr      z, __rintf_zero     ; 0.5 is a tie and goes to 0
    ld      l, #0x80            ; H is already right for 1.0
    ld      de, #0
    ret
__rintf_zero:
    ld      a, h
    and     #0x80
    ld      h, a
    xor     a
    ld      l, a
    ld      d, a
    ld      e, a
    ret
