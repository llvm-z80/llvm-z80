/* Test 18: __smallc - Clang main calls SDCC __smallc functions.
 * expect 0x003F */
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

    /* Bit 0: single argument - the control case, where __smallc and
     * __sdcccall(0) agree. */
    {
        volatile u16 r = sdcc_sc_1i16(41);
        if (r == 42)
            status |= (1 << 0);
    }

    /* Bit 1: two and three arguments.  Reversing the push order would give
     * 201 and 321 instead. */
    {
        volatile u16 r2 = sdcc_sc_2i16(1, 2);
        volatile u16 r3 = sdcc_sc_3i16(1, 2, 3);
        if (r2 == 102 && r3 == 123)
            status |= (1 << 1);
    }

    /* Bit 2: four arguments, so the mirrored offset of the deepest one is
     * three slots away from the nearest. */
    {
        volatile u16 r = sdcc_sc_4i16(1, 2, 3, 4);
        if (r == 1234)
            status |= (1 << 2);
    }

    /* Bit 3: mixed widths - 1, 2 and 1 bytes of stack. */
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

    /* Bit 5: nested __smallc call, and the caller's own frame survives it. */
    {
        volatile u16 canary = 0xBEEF;
        volatile u16 r = sdcc_sc_nested(1, 2, 3);
        if (r == 123 + 301 && canary == 0xBEEF)
            status |= (1 << 5);
    }

    return status; /* expect 0x003F */
}
