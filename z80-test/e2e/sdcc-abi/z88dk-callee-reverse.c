// RUN: rm -rf %t && split-file %s %t
// RUN: %{sdcc} -c %t/sdcc.c -o %t/sdcc.rel
// RUN: %cc %t/clang.c %t/sdcc.rel -Wl,-l,%{sdcc-lib} -o %t/prog
// RUN: %run --expect-result=0x003F %t/prog

//--- clang.c
/* SDCC main calls Clang __z88dk_callee functions with i8, i16 and i32
 * values. Clang is the callee, so Clang pops the stack arguments and reads the
 * first two arguments from the __sdcccall(1) registers. */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

__attribute__((z88dk_callee)) u16 clang_zc_2i16(u16 a, u16 b) {
    return (u16)(a * 100u + b);
}

__attribute__((z88dk_callee)) u16 clang_zc_3i16(u16 a, u16 b, u16 c) {
    return (u16)(a * 100u + b * 10u + c);
}

__attribute__((z88dk_callee)) u16 clang_zc_5i16(u16 a, u16 b, u16 c, u16 d,
                                                u16 e) {
    return (u16)(a * 10000u + b * 1000u + c * 100u + d * 10u + e);
}

__attribute__((z88dk_callee)) u16 clang_zc_i8_i16(u8 a, u16 b) {
    return (u16)(a * 1000u + b);
}

/* Callee cleanup even for a return over 16 bits. Plain __sdcccall(1) cleans
 * up in the caller here, except on SM83. */
__attribute__((z88dk_callee)) u32 clang_zc_i32ret(u16 a, u16 b, u16 c) {
    return (u32)a * 100000UL + (u32)b * 100UL + c;
}

__attribute__((z88dk_callee)) u16 clang_zc_noargs(void) {
    return 0x5A5Au;
}

/* Reads a stack argument after a nested call, to test cleanup across a
 * nested frame. */
__attribute__((z88dk_callee)) u16 clang_zc_nested(u16 a, u16 b, u16 c) {
    u16 inner = clang_zc_3i16(a, b, c);
    return (u16)(inner + c);
}

//--- sdcc.c
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

extern u16 clang_zc_2i16(u16 a, u16 b) __z88dk_callee;
extern u16 clang_zc_3i16(u16 a, u16 b, u16 c) __z88dk_callee;
extern u16 clang_zc_5i16(u16 a, u16 b, u16 c, u16 d, u16 e) __z88dk_callee;
extern u16 clang_zc_i8_i16(u8 a, u16 b) __z88dk_callee;
extern u32 clang_zc_i32ret(u16 a, u16 b, u16 c) __z88dk_callee;
extern u16 clang_zc_noargs(void) __z88dk_callee;
extern u16 clang_zc_nested(u16 a, u16 b, u16 c) __z88dk_callee;

int main(void) {
    u16 status = 0;

    /* Bit 0: two register arguments. */
    {
        u16 r = clang_zc_2i16(1, 2);
        if (r == 102)
            status |= (1 << 0);
    }

    /* Bit 1: 1 and 3 pushed slots. */
    {
        u16 r3 = clang_zc_3i16(1, 2, 3);
        u16 r5 = clang_zc_5i16(1, 2, 3, 4, 5);
        if (r3 == 123 && r5 == 12345)
            status |= (1 << 1);
    }

    /* Bit 2: an i8 first argument. */
    {
        u16 r = clang_zc_i8_i16(9, 876);
        if (r == 9876)
            status |= (1 << 2);
    }

    /* Bit 3: 32-bit return in HLDE (SM83: DEBC), callee cleanup. */
    {
        u32 r = clang_zc_i32ret(6, 54, 321);
        if (r == 605721UL)
            status |= (1 << 3);
    }

    /* Bit 4: no arguments, and a nested call inside the callee. */
    {
        u16 r1 = clang_zc_noargs();
        u16 r2 = clang_zc_nested(1, 2, 3);
        if (r1 == 0x5A5Au && r2 == 126)
            status |= (1 << 4);
    }

    /* Bit 5: repeated calls, so an off-by-one cleanup corrupts the frame. */
    {
        u16 canary = 0xC0DE;
        u16 acc = 0;
        u16 i;
        for (i = 0; i < 8; i++) {
            acc = (u16)(acc + clang_zc_5i16(1, 1, 1, 1, 1));
            acc = (u16)(acc + clang_zc_3i16(0, 0, 1));
        }
        if (acc == 8u * (11111u + 1u) && canary == 0xC0DE && i == 8)
            status |= (1 << 5);
    }

    return status;
}
