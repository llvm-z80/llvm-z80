/* Clang-compiled functions called from SDCC main (reverse direction) */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

u8 clang_i8_i8(u8 a, u8 b) {
    return a + b;
}

u16 clang_i16_i16(u16 a, u16 b) {
    return a + b;
}

u16 clang_3i16(u16 a, u16 b, u16 c) {
    return a + b + c;
}

u32 clang_i32(u32 a) {
    return a + 1;
}

u16 clang_i16_i8(u16 a, u8 b) {
    return a + b;
}

u32 clang_add_u32(u32 a, u32 b) {
    return a + b;
}
