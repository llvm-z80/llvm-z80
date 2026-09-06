/* Test 05: Reverse direction - SDCC main calls Clang functions */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

extern u8 clang_i8_i8(u8 a, u8 b);
extern u16 clang_i16_i16(u16 a, u16 b);
extern u16 clang_3i16(u16 a, u16 b, u16 c);
extern u32 clang_i32(u32 a);
extern u16 clang_i16_i8(u16 a, u8 b);
extern u32 clang_add_u32(u32 a, u32 b);

int main(void) {
    u16 status = 0;

    /* Bit 0: i8 args */
    {
        u8 r = clang_i8_i8(100, 55);
        if (r == 155)
            status |= (1 << 0);
    }

    /* Bit 1: i16 args + mixed */
    {
        u16 r1 = clang_i16_i16(1000, 2000);
        u16 r2 = clang_i16_i8(1000, 5);
        if (r1 == 3000 && r2 == 1005)
            status |= (1 << 1);
    }

    /* Bit 2: stack args (callee-cleanup) */
    {
        u16 r1 = clang_3i16(100, 200, 300);
        u16 r2 = clang_3i16(1000, 2000, 3000);
        if (r1 == 600 && r2 == 6000)
            status |= (1 << 2);
    }

    /* Bit 3: i32 args and return */
    {
        u32 r1 = clang_i32(0x12345677UL);
        u32 r2 = clang_add_u32(0x10000UL, 0x20000UL);
        if (r1 == 0x12345678UL && r2 == 0x30000UL)
            status |= (1 << 3);
    }

    return status; /* expect 0x000F */
}
