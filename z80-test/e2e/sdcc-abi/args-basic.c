// RUN: rm -rf %t && split-file %s %t
// RUN: %{sdcc} -c %t/sdcc.c -o %t/sdcc.rel
// RUN: %cc %t/clang.c %t/sdcc.rel -Wl,-l,%{sdcc-lib} -o %t/prog
// RUN: %run --expect-result=0x000F %t/prog

//--- clang.c
/* Clang main calls SDCC functions with i8, i16 and i32 arguments in the
 * default convention. */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

extern u8 sdcc_i8(u8 a);
extern u8 sdcc_i8_i8(u8 a, u8 b);
extern u16 sdcc_i16(u16 a);
extern u16 sdcc_i16_i16(u16 a, u16 b);
extern u16 sdcc_i8_i16(u8 a, u16 b);
extern u16 sdcc_i16_i8(u16 a, u8 b);
extern u32 sdcc_i32(u32 a);

int main(void) {
    volatile u16 status = 0;

    /* Bit 0: i8 args */
    {
        volatile u8 r1 = sdcc_i8(41);
        volatile u8 r2 = sdcc_i8_i8(100, 55);
        if (r1 == 42 && r2 == 155)
            status |= (1 << 0);
    }

    /* Bit 1: i16 args */
    {
        volatile u16 r1 = sdcc_i16(9999);
        volatile u16 r2 = sdcc_i16_i16(1000, 2000);
        if (r1 == 10000 && r2 == 3000)
            status |= (1 << 1);
    }

    /* Bit 2: mixed i8+i16, i16+i8 */
    {
        volatile u16 r1 = sdcc_i8_i16(5, 1000);
        volatile u16 r2 = sdcc_i16_i8(1000, 5);
        if (r1 == 1005 && r2 == 1005)
            status |= (1 << 2);
    }

    /* Bit 3: i32 arg */
    {
        volatile u32 r1 = sdcc_i32(0x12345677UL);
        if (r1 == 0x12345678UL)
            status |= (1 << 3);
    }

    return status;
}

//--- sdcc.c
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

/* Single i8: A */
u8 sdcc_i8(u8 a) {
    return a + 1;
}

/* Two i8: A, L (SM83: A, E) */
u8 sdcc_i8_i8(u8 a, u8 b) {
    return a + b;
}

/* Single i16: HL (SM83: DE) */
u16 sdcc_i16(u16 a) {
    return a + 1;
}

/* Two i16: HL, DE (SM83: DE, BC) */
u16 sdcc_i16_i16(u16 a, u16 b) {
    return a + b;
}

/* i8 + i16: A, DE */
u16 sdcc_i8_i16(u8 a, u16 b) {
    return a + b;
}

/* i16 + i8: HL, stack (SM83: DE, A) */
u16 sdcc_i16_i8(u16 a, u8 b) {
    return a + b;
}

/* Single i32: HL:DE (SM83: DE:BC) */
u32 sdcc_i32(u32 a) {
    return a + 1;
}
