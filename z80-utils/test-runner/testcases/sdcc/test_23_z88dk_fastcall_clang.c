/* SKIP-IF: sm83 */
/* Test 23: __z88dk_fastcall - Clang main calls SDCC fastcall functions.
 * Z80 only; the SM83 ABI for the convention has never been specified and
 * neither compiler accepts the keyword there.
 * expect 0x000F */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

extern u8 sdcc_fc_i8(u8 a) __attribute__((z88dk_fastcall));
extern u16 sdcc_fc_i16(u16 a) __attribute__((z88dk_fastcall));
extern u32 sdcc_fc_i32(u32 a) __attribute__((z88dk_fastcall));
extern u16 sdcc_fc_i8_widen(u8 a) __attribute__((z88dk_fastcall));
extern u8 sdcc_fc_i32_narrow(u32 a) __attribute__((z88dk_fastcall));
extern u16 sdcc_fc_chain(u16 a) __attribute__((z88dk_fastcall));

int main(void) {
    volatile u16 status = 0;

    /* Bit 0: i8 in L, i16 in HL. */
    {
        volatile u8 r1 = sdcc_fc_i8(41);
        volatile u16 r2 = sdcc_fc_i16(9999);
        if (r1 == 42 && r2 == 10000)
            status |= (1 << 0);
    }

    /* Bit 1: i32 in DEHL, DE holding the high word. */
    {
        volatile u32 r = sdcc_fc_i32(0x11223344UL);
        if (r == 0x11223345UL)
            status |= (1 << 1);
    }

    /* Bit 2: an i8 argument occupies only L, and an i8 return only L. */
    {
        volatile u16 r1 = sdcc_fc_i8_widen(9);
        volatile u8 r2 = sdcc_fc_i32_narrow(0xAB000000UL);
        if (r1 == 900 && r2 == 0xAB)
            status |= (1 << 2);
    }

    /* Bit 3: nested fastcall, and repeated calls to catch a convention that
     * pushed anything on the stack. */
    {
        volatile u16 canary = 0xFACE;
        volatile u16 r = sdcc_fc_chain(1000);
        volatile u16 acc = 0;
        volatile u16 i;
        for (i = 0; i < 8; i++)
            acc = (u16)(acc + sdcc_fc_i16(0));
        if (r == 2002 && acc == 8 && canary == 0xFACE && i == 8)
            status |= (1 << 3);
    }

    return status; /* expect 0x000F */
}
