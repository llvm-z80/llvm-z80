; Test harness startup for the z88dk environment. It stores main's return value
; at _exitcode for the runner to read from a RAM dump.
;
; Assemble with -I<z88dk>/lib for z88dk's section order.

    INCLUDE "crt/classic/crt_section_standard.inc"

    SECTION CODE

    PUBLIC  _start
    PUBLIC  _halt
    PUBLIC  _exitcode
    EXTERN  _main
    EXTERN  __BSS_head
    EXTERN  __BSS_END_tail

_start:
    ld      sp,0                ; SP = 0 wraps to 0xFFFE (top of 64KB RAM)

    ; Zero-fill the BSS sections.
    ld      hl,__BSS_head
    ld      bc,__BSS_END_tail - __BSS_head
    ld      a,b
    or      c
    jr      z,bss_done
    ld      (hl),0
    dec     bc
    ld      a,b
    or      c
    jr      z,bss_done
    ld      d,h
    ld      e,l
    inc     de
    ldir
bss_done:

    ; The tests read argc. Each runs once with no arguments, so argc = 1.
    ld      hl,0                ; argv
    push    hl
    inc     hl                  ; argc
    push    hl
    call    _main
    pop     bc
    pop     bc
    ld      (_exitcode),hl      ; main returns i16 in HL

_halt:
    jr      _halt

    SECTION bss_crt
_exitcode:
    defs    2
