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
    ld      a, e
    rlca
    ld      a, d
    rla                         ; A = exponent
    cp      #150
    ret     nc                  ; |x| >= 2^23, infinity or NaN
    sub     #126
    jr      c, __rintf_zero     ; |x| < 0.5
    jr      z, __rintf_half     ; 0.5 <= |x| < 1
    cpl
    add     a, #24              ; A = number of fraction bits - 1
    ; Move whole fraction bytes out of C, keeping whether they were zero.
    ld      h, #0
__rintf_byte:
    cp      #8
    jr      c, __rintf_bit
    ld      l, a
    ld      a, c
    or      h
    ld      h, a
    ld      c, b
    ld      b, e
    ld      e, d
    ld      d, #0
    ld      a, l
    sub     #8
    jr      __rintf_byte
__rintf_bit:
    ld      l, #1
    or      a
    jr      z, __rintf_round
__rintf_shift:
    sla     l
    dec     a
    jr      nz, __rintf_shift
__rintf_round:
    ; L = the one-half bit of C, H = the moved-out bytes ORed together.
    ld      a, c
    and     l
    jr      z, __rintf_down     ; below one half
    ld      a, l
    dec     a
    and     c
    or      h
    jr      nz, __rintf_up      ; above one half
    ld      a, l
    add     a, a                ; the lowest integer bit
    jr      nc, __rintf_tie
    bit     0, b
    jr      z, __rintf_down
    jr      __rintf_up
__rintf_tie:
    and     c
    jr      z, __rintf_down     ; a tie with an even integer part
__rintf_up:
    ld      a, l
    add     a, a
    dec     a
    or      c
    inc     a                   ; clear the fraction and add one
    ld      c, a
    jr      nz, __rintf_back
    inc     b
    jr      nz, __rintf_back
    inc     e
    jr      nz, __rintf_back
    inc     d
    jr      __rintf_back
__rintf_down:
    ld      a, l
    add     a, a
    dec     a
    cpl
    and     c
    ld      c, a
__rintf_back:
    ; Move the bytes back, filling the fraction bytes with zero.
    ld      a, d
    or      a
    ret     nz
    ld      d, e
    ld      e, b
    ld      b, c
    ld      c, a
    jr      __rintf_back
__rintf_half:
    ld      a, e
    and     #0x7F
    or      b
    or      c
    jr      z, __rintf_zero     ; 0.5 is a tie and goes to 0
    ld      e, #0x80            ; D is already right for 1.0
    ld      bc, #0
    ret
__rintf_zero:
    ld      a, d
    and     #0x80
    ld      d, a
    xor     a
    ld      e, a
    ld      b, a
    ld      c, a
    ret
