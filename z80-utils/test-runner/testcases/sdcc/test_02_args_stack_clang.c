/* Test 02: Stack argument passing - Clang main calls SDCC functions */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

extern u8 sdcc_3i8(u8 a, u8 b, u8 c);
extern u16 sdcc_3i16(u16 a, u16 b, u16 c);
extern u16 sdcc_4i16(u16 a, u16 b, u16 c, u16 d);
extern u16 sdcc_i32_i16(u32 a, u16 b);
extern u8 sdcc_5i8(u8 a, u8 b, u8 c, u8 d, u8 e);
extern u16 sdcc_5i16(u16 a, u16 b, u16 c, u16 d, u16 e);

int main(void) {
    volatile u16 status = 0;

    /* Bit 0: 3 x i8 (1 i8 on stack) */
    {
        volatile u8 r = sdcc_3i8(10, 20, 30);
        if (r == 60)
            status |= (1 << 0);
    }

    /* Bit 1: 3 x i16, 4 x i16 (callee-cleanup) */
    {
        volatile u16 r1 = sdcc_3i16(100, 200, 300);
        volatile u16 r2 = sdcc_4i16(1, 2, 3, 4);
        if (r1 == 600 && r2 == 10)
            status |= (1 << 1);
    }

    /* Bit 2: i32 + i16, 5 x i16 */
    {
        volatile u16 r1 = sdcc_i32_i16(0x10005UL, 10);
        volatile u16 r2 = sdcc_5i16(1, 2, 3, 4, 5);
        if (r1 == 15 && r2 == 15)
            status |= (1 << 2);
    }

    /* Bit 3: 5 x i8 (3 i8 packed on stack) + repeated calls */
    {
        volatile u8 r1 = sdcc_5i8(1, 2, 3, 4, 5);
        volatile u8 r2 = sdcc_5i8(10, 20, 30, 40, 50);
        volatile u16 r3 = sdcc_3i16(1000, 2000, 3000);
        if (r1 == 15 && r2 == 150 && r3 == 6000)
            status |= (1 << 3);
    }

    return status; /* expect 0x000F */
}
