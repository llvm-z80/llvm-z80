/* SKIP-IF: sm83 */
/* Test 24: __z88dk_fastcall reverse - SDCC main calls Clang fastcall
 * functions.  Z80 only; SDCC rejects the keyword for SM83.
 * expect 0x0007 */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

extern u8 clang_fc_i8(u8 a) __z88dk_fastcall;
extern u16 clang_fc_i16(u16 a) __z88dk_fastcall;
extern u32 clang_fc_i32(u32 a) __z88dk_fastcall;
extern u16 clang_fc_i8_widen(u8 a) __z88dk_fastcall;
extern u8 clang_fc_i32_narrow(u32 a) __z88dk_fastcall;

int main(void) {
    u16 status = 0;

    /* Bit 0: i8 in L and i16 in HL.  The i16 case would also pass under the
     * default convention, so it is the i8 that discriminates. */
    {
        u8 r1 = clang_fc_i8(41);
        u16 r2 = clang_fc_i16(9999);
        if (r1 == 42 && r2 == 10000)
            status |= (1 << 0);
    }

    /* Bit 1: i32 in DEHL, and an i32 return in DEHL rather than the default
     * HLDE. */
    {
        u32 r = clang_fc_i32(0x11223344UL);
        if (r == 0x11223345UL)
            status |= (1 << 1);
    }

    /* Bit 2: width changes between the argument and the result. */
    {
        u16 r1 = clang_fc_i8_widen(9);
        u8 r2 = clang_fc_i32_narrow(0xAB000000UL);
        if (r1 == 900 && r2 == 0xAB)
            status |= (1 << 2);
    }

    return status; /* expect 0x0007 */
}
