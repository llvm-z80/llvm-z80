/* Clang-compiled __smallc functions called from SDCC main (reverse).
 * Clang is the CALLEE here, so its mirrored frame offsets have to land where
 * SDCC's left-to-right pushes put the arguments. */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

__attribute__((smallc)) u16 clang_sc_1i16(u16 a) {
    return (u16)(a + 1u);
}

__attribute__((smallc)) u16 clang_sc_2i16(u16 a, u16 b) {
    return (u16)(a * 100u + b);
}

__attribute__((smallc)) u16 clang_sc_3i16(u16 a, u16 b, u16 c) {
    return (u16)(a * 100u + b * 10u + c);
}

__attribute__((smallc)) u16 clang_sc_4i16(u16 a, u16 b, u16 c, u16 d) {
    return (u16)(a * 1000u + b * 100u + c * 10u + d);
}

__attribute__((smallc)) u16 clang_sc_i8_i16_i8(u8 a, u16 b, u8 c) {
    return (u16)(a * 1000u + b * 10u + c);
}

__attribute__((smallc)) u32 clang_sc_i32(u16 a, u16 b) {
    return (u32)a * 100000UL + b;
}

/* Reads its arguments more than once, so a wrong frame offset shows up as an
 * inconsistent value rather than a consistently shifted one. */
__attribute__((smallc)) u16 clang_sc_reread(u16 a, u16 b, u16 c) {
    u16 first = (u16)(a * 100u + b * 10u + c);
    u16 second = (u16)(c * 100u + b * 10u + a);
    return (u16)(first + second);
}
