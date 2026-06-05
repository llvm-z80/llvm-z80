/* Test 11: i64 cross-boundary tests (sret pattern) */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;
typedef unsigned long long u64;

extern u64 sdcc_add64(u64 a, u64 b);
extern u64 sdcc_combine32(u32 hi, u32 lo);
extern u32 sdcc_hi32(u64 val);
extern u32 sdcc_lo32(u64 val);
extern u64 sdcc_shl64(u64 a, u8 shift);
extern u64 sdcc_max64(u64 a, u64 b);

int main(void) {
    volatile u16 status = 0;

    /* Bit 0: i64 compose/decompose */
    {
        volatile u64 r = sdcc_combine32(0xDEADBEEFUL, 0x12345678UL);
        volatile u32 hi = sdcc_hi32(r);
        volatile u32 lo = sdcc_lo32(r);
        if (hi == 0xDEADBEEFUL && lo == 0x12345678UL)
            status |= (1 << 0);
    }

    /* Bit 1: i64 addition */
    {
        volatile u64 r = sdcc_add64(0x00000001ULL, 0x0000FFFFULL);
        volatile u32 lo = sdcc_lo32(r);
        volatile u32 hi = sdcc_hi32(r);
        if (lo == 0x10000UL && hi == 0UL)
            status |= (1 << 1);
    }

    /* Bit 2: i64 addition with carry into high word */
    {
        volatile u64 r = sdcc_add64(0xFFFFFFFFULL, 1ULL);
        volatile u32 hi = sdcc_hi32(r);
        volatile u32 lo = sdcc_lo32(r);
        if (hi == 1UL && lo == 0UL)
            status |= (1 << 2);
    }

    /* Bit 3: i64 shift and max */
    {
        volatile u64 r1 = sdcc_shl64(1ULL, 32);
        volatile u32 hi1 = sdcc_hi32(r1);
        volatile u32 lo1 = sdcc_lo32(r1);
        volatile u64 r2 = sdcc_max64(0x100ULL, 0x200ULL);
        volatile u32 lo2 = sdcc_lo32(r2);
        if (hi1 == 1UL && lo1 == 0UL && lo2 == 0x200UL)
            status |= (1 << 3);
    }

    return status; /* expect 0x000F */
}
