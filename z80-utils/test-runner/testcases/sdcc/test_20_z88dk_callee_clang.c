/* Test 20: __z88dk_callee - Clang main calls SDCC __z88dk_callee functions.
 * expect 0x003F */
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

    /* Bit 1: three and five arguments, so one and three slots get pushed. */
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

    /* Bit 3: 32-bit return, where plain sdcccall(1) would caller-clean.  The
     * value comes back in HLDE. */
    {
        volatile u32 r = sdcc_zc_i32ret(6, 54, 321);
        if (r == 605721UL)
            status |= (1 << 3);
    }

    /* Bit 4: zero arguments must not produce a spurious pop, and a void
     * return still cleans its stack slots. */
    {
        volatile u16 out = 0;
        volatile u16 r = sdcc_zc_noargs();
        sdcc_zc_void(1, 2, 3, (u16 *)&out);
        if (r == 0x5A5Au && out == 123)
            status |= (1 << 4);
    }

    /* Bit 5: a run of calls with stack arguments.  One pop too many or too
     * few per call accumulates, so the canary below and the loop counter are
     * what actually catch a cleanup mismatch. */
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

    return status; /* expect 0x003F */
}
