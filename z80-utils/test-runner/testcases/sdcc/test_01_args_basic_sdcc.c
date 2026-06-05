/* SDCC-compiled functions for basic argument passing tests */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

/* Single i8: A */
u8 sdcc_i8(u8 a) {
    return a + 1;
}

/* Two i8: Z80(A,L) SM83(A,E) */
u8 sdcc_i8_i8(u8 a, u8 b) {
    return a + b;
}

/* Single i16: Z80(HL) SM83(DE) */
u16 sdcc_i16(u16 a) {
    return a + 1;
}

/* Two i16: Z80(HL,DE) SM83(DE,BC) */
u16 sdcc_i16_i16(u16 a, u16 b) {
    return a + b;
}

/* i8 + i16: Z80(A,DE) SM83(A,DE) */
u16 sdcc_i8_i16(u8 a, u16 b) {
    return a + b;
}

/* i16 + i8: Z80(HL,stack) SM83(DE,A) */
u16 sdcc_i16_i8(u16 a, u8 b) {
    return a + b;
}

/* Single i32: Z80(HL:DE) SM83(DE:BC) */
u32 sdcc_i32(u32 a) {
    return a + 1;
}
