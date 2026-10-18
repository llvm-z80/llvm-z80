// RUN: rm -rf %t && split-file %s %t
// RUN: %{sdcc} -c %t/sdcc.c -o %t/sdcc.rel
// RUN: %cc %t/clang.c %t/sdcc.rel -Wl,-l,%{sdcc-lib} -o %t/prog
// RUN: %run --expect-result=0x003F %t/prog

//--- clang.c
/* Clang main calls SDCC __z88dk_callee functions with i8, i16 and i32
 * values. __z88dk_callee keeps the __sdcccall(1) argument registers and only
 * moves stack cleanup to the callee, so the checks look for cleanup done
 * exactly once. */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

extern u16 sdcc_zc_2i16(u16 a, u16 b) __attribute__((z88dk_callee));
extern u16 sdcc_zc_3i16(u16 a, u16 b, u16 c) __attribute__((z88dk_callee));
extern u16 sdcc_zc_5i16(u16 a, u16 b, u16 c, u16 d, u16 e)
    __attribute__((z88dk_callee));
extern u16 sdcc_zc_i8_i16(u8 a, u16 b) __attribute__((z88dk_callee));
extern u32 sdcc_zc_i32ret(u16 a, u16 b, u16 c) __attribute__((z88dk_callee));
extern u16 sdcc_zc_noargs(void) __attribute__((z88dk_callee));
extern void sdcc_zc_void(u16 a, u16 b, u16 c, u16 *out)
    __attribute__((z88dk_callee));

int main(void) {
    volatile u16 status = 0;

    /* Bit 0: two arguments, both in registers, nothing to clean up. */
    {
        volatile u16 r = sdcc_zc_2i16(1, 2);
        if (r == 102)
            status |= (1 << 0);
    }

    /* Bit 1: three and five arguments, with 1 and 3 slots pushed. */
    {
        volatile u16 r3 = sdcc_zc_3i16(1, 2, 3);
        volatile u16 r5 = sdcc_zc_5i16(1, 2, 3, 4, 5);
        if (r3 == 123 && r5 == 12345)
            status |= (1 << 1);
    }

    /* Bit 2: an i8 first argument shifts the register assignment. */
    {
        volatile u16 r = sdcc_zc_i8_i16(9, 876);
        if (r == 9876)
            status |= (1 << 2);
    }

    /* Bit 3: 32-bit return in HLDE (SM83: DEBC). */
    {
        volatile u32 r = sdcc_zc_i32ret(6, 54, 321);
        if (r == 605721UL)
            status |= (1 << 3);
    }

    /* Bit 4: zero arguments with no spurious pop, and a void return that
     * still cleans the stack slots. */
    {
        volatile u16 out = 0;
        volatile u16 r = sdcc_zc_noargs();
        sdcc_zc_void(1, 2, 3, (u16 *)&out);
        if (r == 0x5A5Au && out == 123)
            status |= (1 << 4);
    }

    /* Bit 5: a run of calls with stack arguments. The canary and the loop
     * counter catch a cleanup mismatch. */
    {
        volatile u16 canary = 0xC0DE;
        volatile u16 acc = 0;
        volatile u16 i;
        for (i = 0; i < 8; i++) {
            acc = (u16)(acc + sdcc_zc_5i16(1, 1, 1, 1, 1));
            acc = (u16)(acc + sdcc_zc_3i16(0, 0, 1));
        }
        if (acc == 8u * (11111u + 1u) && canary == 0xC0DE && i == 8)
            status |= (1 << 5);
    }

    return status;
}

//--- sdcc.c
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

u16 sdcc_zc_2i16(u16 a, u16 b) __z88dk_callee {
    return (u16)(a * 100u + b);
}

u16 sdcc_zc_3i16(u16 a, u16 b, u16 c) __z88dk_callee {
    return (u16)(a * 100u + b * 10u + c);
}

u16 sdcc_zc_5i16(u16 a, u16 b, u16 c, u16 d, u16 e) __z88dk_callee {
    return (u16)(a * 10000u + b * 1000u + c * 100u + d * 10u + e);
}

u16 sdcc_zc_i8_i16(u8 a, u16 b) __z88dk_callee {
    return (u16)(a * 1000u + b);
}

/* Plain __sdcccall(1) cleans up in the caller for a 32-bit return, except on
 * SM83. */
u32 sdcc_zc_i32ret(u16 a, u16 b, u16 c) __z88dk_callee {
    return (u32)a * 100000UL + (u32)b * 100UL + c;
}

/* No stack argument: nothing to clean, so no pop. */
u16 sdcc_zc_noargs(void) __z88dk_callee {
    return 0x5A5Au;
}

void sdcc_zc_void(u16 a, u16 b, u16 c, u16 *out) __z88dk_callee {
    *out = (u16)(a * 100u + b * 10u + c);
}
