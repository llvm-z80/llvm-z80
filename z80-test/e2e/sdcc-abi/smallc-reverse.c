// RUN: rm -rf %t && split-file %s %t
// RUN: %{sdcc} -c %t/sdcc.c -o %t/sdcc.rel
// RUN: %cc %t/clang.c %t/sdcc.rel -Wl,-l,%{sdcc-lib} -o %t/prog
// RUN: %run --expect-result=0x003F %t/prog

//--- clang.c
/* SDCC main calls Clang __smallc functions with i8, i16 and i32 values.
 * Clang is the callee, so the frame offsets of Clang have to match the
 * left-to-right pushes of SDCC. */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

__attribute__((smallc)) u16 clang_sc_1i16(u16 a) {
    return (u16)(a + 1u);
}

__attribute__((smallc)) u16 clang_sc_2i16(u16 a, u16 b) {
    return (u16)(a * 100u + b);
}

__attribute__((smallc)) u16 clang_sc_3i16(u16 a, u16 b, u16 c) {
    return (u16)(a * 100u + b * 10u + c);
}

__attribute__((smallc)) u16 clang_sc_4i16(u16 a, u16 b, u16 c, u16 d) {
    return (u16)(a * 1000u + b * 100u + c * 10u + d);
}

__attribute__((smallc)) u16 clang_sc_i8_i16_i8(u8 a, u16 b, u8 c) {
    return (u16)(a * 1000u + b * 10u + c);
}

__attribute__((smallc)) u32 clang_sc_i32(u16 a, u16 b) {
    return (u32)a * 100000UL + b;
}

/* Reads each argument twice: a wrong frame offset breaks the relation between
 * the two sums. */
__attribute__((smallc)) u16 clang_sc_reread(u16 a, u16 b, u16 c) {
    u16 first = (u16)(a * 100u + b * 10u + c);
    u16 second = (u16)(c * 100u + b * 10u + a);
    return (u16)(first + second);
}

//--- sdcc.c
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

extern u16 clang_sc_1i16(u16 a) __smallc;
extern u16 clang_sc_2i16(u16 a, u16 b) __smallc;
extern u16 clang_sc_3i16(u16 a, u16 b, u16 c) __smallc;
extern u16 clang_sc_4i16(u16 a, u16 b, u16 c, u16 d) __smallc;
extern u16 clang_sc_i8_i16_i8(u8 a, u16 b, u8 c) __smallc;
extern u32 clang_sc_i32(u16 a, u16 b) __smallc;
extern u16 clang_sc_reread(u16 a, u16 b, u16 c) __smallc;

int main(void) {
    u16 status = 0;

    /* Bit 0: single argument, the control case. */
    {
        u16 r = clang_sc_1i16(41);
        if (r == 42)
            status |= (1 << 0);
    }

    /* Bit 1: two and three arguments. */
    {
        u16 r2 = clang_sc_2i16(1, 2);
        u16 r3 = clang_sc_3i16(1, 2, 3);
        if (r2 == 102 && r3 == 123)
            status |= (1 << 1);
    }

    /* Bit 2: four arguments. */
    {
        u16 r = clang_sc_4i16(1, 2, 3, 4);
        if (r == 1234)
            status |= (1 << 2);
    }

    /* Bit 3: mixed widths. */
    {
        u16 r = clang_sc_i8_i16_i8(9, 87, 6);
        if (r == 9876)
            status |= (1 << 3);
    }

    /* Bit 4: i32 return in DEHL. */
    {
        u32 r = clang_sc_i32(6, 54321);
        if (r == 654321UL)
            status |= (1 << 4);
    }

    /* Bit 5: arguments read twice in the callee, then caller cleanup of the
     * three slots. */
    {
        u16 canary = 0xBEEF;
        u16 r = clang_sc_reread(1, 2, 3);
        if (r == 123 + 321 && canary == 0xBEEF)
            status |= (1 << 5);
    }

    return status;
}
