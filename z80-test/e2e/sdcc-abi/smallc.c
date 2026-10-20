// RUN: rm -rf %t && split-file %s %t
// RUN: %{sdcc} -c %t/sdcc.c -o %t/sdcc.rel
// RUN: %cc %t/clang.c %t/sdcc.rel -Wl,-l,%{sdcc-lib} -o %t/prog
// RUN: %run --expect-result=0x003F %t/prog

//--- clang.c
/* Clang main calls SDCC __smallc functions with i8, i16 and i32 values.
 * __smallc pushes the arguments left to right, so each SDCC function encodes
 * the argument order in the result. */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

extern u16 sdcc_sc_1i16(u16 a) __attribute__((smallc));
extern u16 sdcc_sc_2i16(u16 a, u16 b) __attribute__((smallc));
extern u16 sdcc_sc_3i16(u16 a, u16 b, u16 c) __attribute__((smallc));
extern u16 sdcc_sc_4i16(u16 a, u16 b, u16 c, u16 d) __attribute__((smallc));
extern u16 sdcc_sc_i8_i16_i8(u8 a, u16 b, u8 c) __attribute__((smallc));
extern u32 sdcc_sc_i32(u16 a, u16 b) __attribute__((smallc));
extern u16 sdcc_sc_nested(u16 a, u16 b, u16 c) __attribute__((smallc));

int main(void) {
    volatile u16 status = 0;

    /* Bit 0: single argument, the control case: __smallc and __sdcccall(0)
     * agree. */
    {
        volatile u16 r = sdcc_sc_1i16(41);
        if (r == 42)
            status |= (1 << 0);
    }

    /* Bit 1: two and three arguments. */
    {
        volatile u16 r2 = sdcc_sc_2i16(1, 2);
        volatile u16 r3 = sdcc_sc_3i16(1, 2, 3);
        if (r2 == 102 && r3 == 123)
            status |= (1 << 1);
    }

    /* Bit 2: four arguments. */
    {
        volatile u16 r = sdcc_sc_4i16(1, 2, 3, 4);
        if (r == 1234)
            status |= (1 << 2);
    }

    /* Bit 3: mixed widths i8, i16, i8, each in a two-byte slot. */
    {
        volatile u16 r = sdcc_sc_i8_i16_i8(9, 87, 6);
        if (r == 9876)
            status |= (1 << 3);
    }

    /* Bit 4: i32 return in DEHL. */
    {
        volatile u32 r = sdcc_sc_i32(6, 54321);
        if (r == 654321UL)
            status |= (1 << 4);
    }

    /* Bit 5: nested __smallc call, with a canary in the caller frame. */
    {
        volatile u16 canary = 0xBEEF;
        volatile u16 r = sdcc_sc_nested(1, 2, 3);
        if (r == 123 + 301 && canary == 0xBEEF)
            status |= (1 << 5);
    }

    return status;
}

//--- sdcc.c
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

u16 sdcc_sc_2i16(u16 a, u16 b) __smallc {
    return (u16)(a * 100u + b);
}

u16 sdcc_sc_3i16(u16 a, u16 b, u16 c) __smallc {
    return (u16)(a * 100u + b * 10u + c);
}

u16 sdcc_sc_4i16(u16 a, u16 b, u16 c, u16 d) __smallc {
    return (u16)(a * 1000u + b * 100u + c * 10u + d);
}

/* Control case for the three functions above. */
u16 sdcc_sc_1i16(u16 a) __smallc {
    return (u16)(a + 1u);
}

/* Mixed widths: each i8 takes a full two-byte slot under __smallc. */
u16 sdcc_sc_i8_i16_i8(u8 a, u16 b, u8 c) __smallc {
    return (u16)(a * 1000u + b * 10u + c);
}

/* i32 return in DEHL, unlike HLDE under __sdcccall(1). */
u32 sdcc_sc_i32(u16 a, u16 b) __smallc {
    return (u32)a * 100000UL + b;
}

/* Nested __smallc calls, to test the order on a nested frame. */
u16 sdcc_sc_nested(u16 a, u16 b, u16 c) __smallc {
    return (u16)(sdcc_sc_3i16(a, b, c) + sdcc_sc_2i16(c, a));
}
