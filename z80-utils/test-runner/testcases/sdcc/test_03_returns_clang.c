/* Test 03: Return value tests - Clang main calls SDCC functions */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

extern u8 sdcc_ret_u8(u16 a);
extern u16 sdcc_ret_u16(u8 hi, u8 lo);
extern u32 sdcc_ret_u32(u16 hi, u16 lo);
extern u32 sdcc_add_u32(u32 a, u32 b);
extern u8 sdcc_max3_u8(u8 a, u8 b, u8 c);

int main(void) {
    volatile u16 status = 0;

    /* Bit 0: i8 return */
    {
        volatile u8 r1 = sdcc_ret_u8(0x1234);
        volatile u8 r2 = sdcc_max3_u8(10, 50, 30);
        if (r1 == 0x34 && r2 == 50)
            status |= (1 << 0);
    }

    /* Bit 1: i16 return */
    {
        volatile u16 r1 = sdcc_ret_u16(0xAB, 0xCD);
        if (r1 == 0xABCD)
            status |= (1 << 1);
    }

    /* Bit 2: i32 return from i16 inputs */
    {
        volatile u32 r1 = sdcc_ret_u32(0x1234, 0x5678);
        if (r1 == 0x12345678UL)
            status |= (1 << 2);
    }

    /* Bit 3: i32 return from i32 inputs (stack) */
    {
        volatile u32 r1 = sdcc_add_u32(0x10000UL, 0x20000UL);
        volatile u32 r2 = sdcc_add_u32(0xFFFF0000UL, 0x10000UL);
        if (r1 == 0x30000UL && r2 == 0UL)
            status |= (1 << 3);
    }

    return status; /* expect 0x000F */
}
