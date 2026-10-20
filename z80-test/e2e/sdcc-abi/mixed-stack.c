// RUN: rm -rf %t && split-file %s %t
// RUN: %{sdcc} -c %t/sdcc.c -o %t/sdcc.rel
// RUN: %cc %t/clang.c %t/sdcc.rel -Wl,-l,%{sdcc-lib} -o %t/prog
// RUN: %run --expect-result=0x000F %t/prog

//--- clang.c
/* Clang main calls SDCC functions with mixed i8, i16 and i32 arguments,
 * some on the stack. */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

extern u16 sdcc_mix_i8i8i16i8(u8 a, u8 b, u16 c, u8 d);
extern u16 sdcc_mix_i16i16i8i16(u16 a, u16 b, u8 c, u16 d);
extern u16 sdcc_i32_i16_i8(u32 a, u16 b, u8 c);
extern u16 sdcc_6i16(u16 a, u16 b, u16 c, u16 d, u16 e, u16 f);
extern u8 sdcc_8i8(u8 a, u8 b, u8 c, u8 d, u8 e, u8 f, u8 g, u8 h);
extern u16 sdcc_alt_types(u16 a, u8 b, u16 c, u8 d, u16 e);

int main(void) {
    volatile u16 status = 0;

    /* Bit 0: mixed i8+i16 on stack */
    {
        volatile u16 r1 = sdcc_mix_i8i8i16i8(1, 2, 300, 4);
        volatile u16 r2 = sdcc_mix_i16i16i8i16(100, 200, 3, 400);
        if (r1 == 307 && r2 == 703)
            status |= (1 << 0);
    }

    /* Bit 1: i32 + mixed stack args */
    {
        volatile u16 r = sdcc_i32_i16_i8(0x10042UL, 100, 5);
        if (r == 171)  /* 0x42 + 100 + 5 = 171 */
            status |= (1 << 1);
    }

    /* Bit 2: 6 x i16, 8 x i8 */
    {
        volatile u16 r1 = sdcc_6i16(1, 2, 3, 4, 5, 6);
        volatile u8 r2 = sdcc_8i8(1, 2, 3, 4, 5, 6, 7, 8);
        if (r1 == 21 && r2 == 36)
            status |= (1 << 2);
    }

    /* Bit 3: alternating types + repeated calls */
    {
        volatile u16 r1 = sdcc_alt_types(1000, 5, 2000, 10, 3000);
        volatile u16 r2 = sdcc_alt_types(100, 1, 200, 2, 300);
        if (r1 == 6015 && r2 == 603)
            status |= (1 << 3);
    }

    return status;
}

//--- sdcc.c
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

/* i8, i8, i16, i8: 2 in registers, 1 i16 and 1 i8 on stack */
u16 sdcc_mix_i8i8i16i8(u8 a, u8 b, u16 c, u8 d) {
    return a + b + c + d;
}

/* i16, i16, i8, i16: 2 in registers, 1 i8 and 1 i16 on stack */
u16 sdcc_mix_i16i16i8i16(u16 a, u16 b, u8 c, u16 d) {
    return a + b + c + d;
}

/* i32, i16, i8: i32 in registers, i16 and i8 on stack */
u16 sdcc_i32_i16_i8(u32 a, u16 b, u8 c) {
    return (u16)(a & 0xFF) + b + c;
}

/* 6 x i16: 2 in registers, 4 on stack */
u16 sdcc_6i16(u16 a, u16 b, u16 c, u16 d, u16 e, u16 f) {
    return a + b + c + d + e + f;
}

/* 8 x i8: 2 in registers, 6 packed i8 on stack */
u8 sdcc_8i8(u8 a, u8 b, u8 c, u8 d, u8 e, u8 f, u8 g, u8 h) {
    return a + b + c + d + e + f + g + h;
}

/* i16, i8, i16, i8, i16: alternating types on stack */
u16 sdcc_alt_types(u16 a, u8 b, u16 c, u8 d, u16 e) {
    return a + b + c + d + e;
}
