/* SDCC-compiled sdcccall(0) functions for ABI compatibility tests */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

/* sdcccall(0): all args on stack, return i8->L, i16->HL, i32->DEHL */

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
