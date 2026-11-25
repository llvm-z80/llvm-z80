// RUN: rm -rf %t && split-file %s %t
// RUN: %{sdcc} -c %t/sdcc.c -o %t/sdcc.rel
// RUN: %cc %t/clang.c %t/sdcc.rel -o %t/prog
// RUN: %run --expect-result=0x0007 %t/prog

//--- clang.c
/* Clang main calls SDCC assembly that keeps only the listed registers and
 * overwrites the others, so a value kept in any other register breaks. */
typedef unsigned char u8;

extern void keep_bc(void) __attribute__((preserves_regs(b, c)));
extern void keep_dehl(void) __attribute__((preserves_regs(d, e, h, l)));
extern void keep_a(void) __attribute__((preserves_regs(a)));

volatile u8 seed = 1;

int main(void) {
    volatile unsigned status = 0;
    u8 v0 = seed, v1 = seed + 1, v2 = seed + 2, v3 = seed + 3, v4 = seed + 4;

    /* Bit 0: B and C survive */
    keep_bc();
    if ((u8)(v0 + v1 + v2 + v3 + v4) == 15)
        status |= (1 << 0);

    /* Bit 1: D, E, H and L survive */
    keep_dehl();
    if ((u8)(v0 * v1 + v2 * v3 + v4) == 19)
        status |= (1 << 1);

    /* Bit 2: A survives */
    keep_a();
    if ((u8)(v4 - v0 + v2) == 7)
        status |= (1 << 2);

    return status;
}

//--- sdcc.c
void keep_bc(void) __preserves_regs(b, c) __naked
{
    __asm
    ld a, #0x5a
    ld de, #0x1234
    ld hl, #0x5678
#ifndef __SDCC_sm83
    ld iy, #0x9abc
#endif
    ret
    __endasm;
}

void keep_dehl(void) __preserves_regs(d, e, h, l) __naked
{
    __asm
    ld a, #0x5a
    ld bc, #0x1234
#ifndef __SDCC_sm83
    ld iy, #0x9abc
#endif
    ret
    __endasm;
}

void keep_a(void) __preserves_regs(a) __naked
{
    __asm
    ld bc, #0x1111
    ld de, #0x2222
    ld hl, #0x3333
#ifndef __SDCC_sm83
    ld iy, #0x4444
#endif
    ret
    __endasm;
}
