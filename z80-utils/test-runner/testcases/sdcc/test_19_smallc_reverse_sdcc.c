/* Test 19: __smallc reverse - SDCC main calls Clang __smallc functions.
 * expect 0x003F */
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

    /* Bit 0: single argument - the control case. */
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

    /* Bit 5: arguments read twice inside the callee, and the caller cleans up
     * afterwards - SDCC pops the three slots itself under __smallc. */
    {
        u16 canary = 0xBEEF;
        u16 r = clang_sc_reread(1, 2, 3);
        if (r == 123 + 321 && canary == 0xBEEF)
            status |= (1 << 5);
    }

    return status; /* expect 0x003F */
}
