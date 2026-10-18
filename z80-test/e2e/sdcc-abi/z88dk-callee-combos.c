// RUN: rm -rf %t && split-file %s %t
// RUN: %{sdcc} -c %t/sdcc.c -o %t/sdcc.rel
// RUN: %cc %t/clang.c %t/sdcc.rel -Wl,-l,%{sdcc-lib} -o %t/prog
// RUN: %run --expect-result=0x001F %t/prog

//--- clang.c
/* Clang main calls SDCC __sdcccall(0) __z88dk_callee and __smallc
 * __z88dk_callee functions with i8, i16 and i32 values. Each combination
 * keeps the stack layout of the base convention and cleans up in the callee. */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

extern u16 sdcc_c0c_3i16(u16 a, u16 b, u16 c)
    __attribute__((sdcccall(0))) __attribute__((z88dk_callee));
extern u16 sdcc_c0c_4i16(u16 a, u16 b, u16 c, u16 d)
    __attribute__((sdcccall(0))) __attribute__((z88dk_callee));
extern u32 sdcc_c0c_i32(u16 a, u16 b)
    __attribute__((sdcccall(0))) __attribute__((z88dk_callee));

extern u16 sdcc_scc_3i16(u16 a, u16 b, u16 c)
    __attribute__((smallc)) __attribute__((z88dk_callee));
/* Written in the other order: the composition is order-independent. */
extern u16 sdcc_scc_4i16(u16 a, u16 b, u16 c, u16 d)
    __attribute__((z88dk_callee)) __attribute__((smallc));
extern u16 sdcc_scc_i8_i16_i8(u8 a, u16 b, u8 c)
    __attribute__((smallc)) __attribute__((z88dk_callee));
extern u32 sdcc_scc_i32(u16 a, u16 b)
    __attribute__((smallc)) __attribute__((z88dk_callee));

int main(void) {
    volatile u16 status = 0;

    /* Bit 0: __sdcccall(0) __z88dk_callee, right-to-left push. */
    {
        volatile u16 r3 = sdcc_c0c_3i16(1, 2, 3);
        volatile u16 r4 = sdcc_c0c_4i16(1, 2, 3, 4);
        if (r3 == 123 && r4 == 1234)
            status |= (1 << 0);
    }

    /* Bit 1: __sdcccall(0) __z88dk_callee, i32 return in DEHL. */
    {
        volatile u32 r = sdcc_c0c_i32(6, 54321);
        if (r == 654321UL)
            status |= (1 << 1);
    }

    /* Bit 2: __smallc __z88dk_callee, left-to-right push, same arguments as
     * bit 0. */
    {
        volatile u16 r3 = sdcc_scc_3i16(1, 2, 3);
        volatile u16 r4 = sdcc_scc_4i16(1, 2, 3, 4);
        if (r3 == 123 && r4 == 1234)
            status |= (1 << 2);
    }

    /* Bit 3: __smallc __z88dk_callee with mixed widths and an i32 return. */
    {
        volatile u16 r1 = sdcc_scc_i8_i16_i8(9, 87, 6);
        volatile u32 r2 = sdcc_scc_i32(6, 54321);
        if (r1 == 9876 && r2 == 654321UL)
            status |= (1 << 3);
    }

    /* Bit 4: repeated calls across both combinations, so a missing callee pop
     * leaks the stack. */
    {
        volatile u16 canary = 0xF00D;
        volatile u16 acc = 0;
        volatile u16 i;
        for (i = 0; i < 8; i++) {
            acc = (u16)(acc + sdcc_c0c_3i16(0, 0, 1));
            acc = (u16)(acc + sdcc_scc_3i16(0, 0, 1));
        }
        if (acc == 16 && canary == 0xF00D && i == 8)
            status |= (1 << 4);
    }

    return status;
}

//--- sdcc.c
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

u16 sdcc_c0c_3i16(u16 a, u16 b, u16 c) __sdcccall(0) __z88dk_callee {
    return (u16)(a * 100u + b * 10u + c);
}

u16 sdcc_c0c_4i16(u16 a, u16 b, u16 c, u16 d) __sdcccall(0) __z88dk_callee {
    return (u16)(a * 1000u + b * 100u + c * 10u + d);
}

u32 sdcc_c0c_i32(u16 a, u16 b) __sdcccall(0) __z88dk_callee {
    return (u32)a * 100000UL + b;
}

u16 sdcc_scc_3i16(u16 a, u16 b, u16 c) __smallc __z88dk_callee {
    return (u16)(a * 100u + b * 10u + c);
}

u16 sdcc_scc_4i16(u16 a, u16 b, u16 c, u16 d) __smallc __z88dk_callee {
    return (u16)(a * 1000u + b * 100u + c * 10u + d);
}

u16 sdcc_scc_i8_i16_i8(u8 a, u16 b, u8 c) __smallc __z88dk_callee {
    return (u16)(a * 1000u + b * 10u + c);
}

u32 sdcc_scc_i32(u16 a, u16 b) __smallc __z88dk_callee {
    return (u32)a * 100000UL + b;
}
