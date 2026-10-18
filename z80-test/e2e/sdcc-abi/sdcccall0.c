// RUN: rm -rf %t && split-file %s %t
// RUN: %{sdcc} -c %t/sdcc.c -o %t/sdcc.rel
// RUN: %cc %t/clang.c %t/sdcc.rel -Wl,-l,%{sdcc-lib} -o %t/prog
// RUN: %run --expect-result=0x000F %t/prog

//--- clang.c
/* Clang main calls SDCC __sdcccall(0) functions with i8, i16 and i32
 * arguments. */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

extern u8 sdcc_cc0_i8(u8 a) __attribute__((sdcccall(0)));
extern u8 sdcc_cc0_i8_i8(u8 a, u8 b) __attribute__((sdcccall(0)));
extern u16 sdcc_cc0_i16(u16 a) __attribute__((sdcccall(0)));
extern u16 sdcc_cc0_i16_i16(u16 a, u16 b) __attribute__((sdcccall(0)));
extern u16 sdcc_cc0_i8_i16(u8 a, u16 b) __attribute__((sdcccall(0)));
extern u32 sdcc_cc0_i32(u32 a) __attribute__((sdcccall(0)));
extern u16 sdcc_cc0_3i16(u16 a, u16 b, u16 c) __attribute__((sdcccall(0)));

int main(void) {
    volatile u16 status = 0;

    /* Bit 0: i8 args (all on stack) */
    {
        volatile u8 r1 = sdcc_cc0_i8(41);
        volatile u8 r2 = sdcc_cc0_i8_i8(100, 55);
        if (r1 == 42 && r2 == 155)
            status |= (1 << 0);
    }

    /* Bit 1: i16 args (all on stack) */
    {
        volatile u16 r1 = sdcc_cc0_i16(9999);
        volatile u16 r2 = sdcc_cc0_i16_i16(1000, 2000);
        if (r1 == 10000 && r2 == 3000)
            status |= (1 << 1);
    }

    /* Bit 2: mixed i8 and i16, 3 args */
    {
        volatile u16 r1 = sdcc_cc0_i8_i16(5, 1000);
        volatile u16 r2 = sdcc_cc0_3i16(100, 200, 300);
        if (r1 == 1005 && r2 == 600)
            status |= (1 << 2);
    }

    /* Bit 3: i32 arg */
    {
        volatile u32 r1 = sdcc_cc0_i32(0x12345677UL);
        if (r1 == 0x12345678UL)
            status |= (1 << 3);
    }

    return status;
}

//--- sdcc.c
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

/* __sdcccall(0): all args on stack, return i8 in L, i16 in HL, i32 in DEHL */

u8 sdcc_cc0_i8(u8 a) __sdcccall(0) {
    return a + 1;
}

u8 sdcc_cc0_i8_i8(u8 a, u8 b) __sdcccall(0) {
    return a + b;
}

u16 sdcc_cc0_i16(u16 a) __sdcccall(0) {
    return a + 1;
}

u16 sdcc_cc0_i16_i16(u16 a, u16 b) __sdcccall(0) {
    return a + b;
}

u16 sdcc_cc0_i8_i16(u8 a, u16 b) __sdcccall(0) {
    return a + b;
}

u32 sdcc_cc0_i32(u32 a) __sdcccall(0) {
    return a + 1;
}

u16 sdcc_cc0_3i16(u16 a, u16 b, u16 c) __sdcccall(0) {
    return a + b + c;
}
