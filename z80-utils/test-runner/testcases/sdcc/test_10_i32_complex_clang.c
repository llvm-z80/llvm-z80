/* Test 10: Complex i32 argument and return patterns */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

extern u32 sdcc_add32(u32 a, u32 b);
extern u32 sdcc_mul32_add(u32 a, u32 b, u16 c);
extern u32 sdcc_shl32(u32 a, u8 shift);
extern u32 sdcc_max32(u32 a, u32 b);
extern u32 sdcc_combine16(u16 hi, u16 lo);
extern u16 sdcc_hi16(u32 val);
extern u16 sdcc_lo16(u32 val);

int main(void) {
    volatile u16 status = 0;

    /* Bit 0: i32 add, combine/decompose */
    {
        volatile u32 r1 = sdcc_add32(0x10000UL, 0x20000UL);
        volatile u32 r2 = sdcc_combine16(0xABCD, 0x1234);
        volatile u16 r3 = sdcc_hi16(0xDEADBEEFUL);
        volatile u16 r4 = sdcc_lo16(0xDEADBEEFUL);
        if (r1 == 0x30000UL && r2 == 0xABCD1234UL &&
            r3 == 0xDEAD && r4 == 0xBEEF)
            status |= (1 << 0);
    }

    /* Bit 1: i32 shift */
    {
        volatile u32 r1 = sdcc_shl32(1UL, 16);
        volatile u32 r2 = sdcc_shl32(0xFFUL, 8);
        if (r1 == 0x10000UL && r2 == 0xFF00UL)
            status |= (1 << 1);
    }

    /* Bit 2: i32 max (conditional return) */
    {
        volatile u32 r1 = sdcc_max32(0x12345678UL, 0x12345677UL);
        volatile u32 r2 = sdcc_max32(100UL, 200UL);
        if (r1 == 0x12345678UL && r2 == 200UL)
            status |= (1 << 2);
    }

    /* Bit 3: chained i32 operations */
    {
        volatile u32 a = sdcc_combine16(0x0001, 0x0000);  /* 0x10000 */
        volatile u32 b = sdcc_add32(a, 0x5678UL);         /* 0x15678 */
        volatile u16 hi = sdcc_hi16(b);
        volatile u16 lo = sdcc_lo16(b);
        if (hi == 0x0001 && lo == 0x5678)
            status |= (1 << 3);
    }

    return status; /* expect 0x000F */
}
