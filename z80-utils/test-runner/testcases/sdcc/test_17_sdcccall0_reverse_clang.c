/* Clang-compiled sdcccall(0) functions called from SDCC main (reverse) */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

__attribute__((sdcccall(0))) u8 clang_cc0_i8_i8(u8 a, u8 b) {
    return a + b;
}

__attribute__((sdcccall(0))) u16 clang_cc0_i16_i16(u16 a, u16 b) {
    return a + b;
}

__attribute__((sdcccall(0))) u16 clang_cc0_3i16(u16 a, u16 b, u16 c) {
    return a + b + c;
}

__attribute__((sdcccall(0))) u32 clang_cc0_i32(u32 a) {
    return a + 1;
}

__attribute__((sdcccall(0))) u16 clang_cc0_i16_i8(u16 a, u8 b) {
    return a + b;
}

__attribute__((sdcccall(0))) u32 clang_cc0_add_u32(u32 a, u32 b) {
    return a + b;
}
