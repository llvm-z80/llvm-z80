/* Clang-compiled __z88dk_callee functions called from SDCC main (reverse).
 * Clang is the CALLEE here, so it is the side that has to pop the stack
 * arguments, and it has to read them from where SDCC's __sdcccall(1)
 * register split leaves them. */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

__attribute__((z88dk_callee)) u16 clang_zc_2i16(u16 a, u16 b) {
    return (u16)(a * 100u + b);
}

__attribute__((z88dk_callee)) u16 clang_zc_3i16(u16 a, u16 b, u16 c) {
    return (u16)(a * 100u + b * 10u + c);
}

__attribute__((z88dk_callee)) u16 clang_zc_5i16(u16 a, u16 b, u16 c, u16 d,
                                                u16 e) {
    return (u16)(a * 10000u + b * 1000u + c * 100u + d * 10u + e);
}

__attribute__((z88dk_callee)) u16 clang_zc_i8_i16(u8 a, u16 b) {
    return (u16)(a * 1000u + b);
}

/* Cleanup is forced here even though the return exceeds 16 bits, which is the
 * one case where this differs from plain __sdcccall(1). */
__attribute__((z88dk_callee)) u32 clang_zc_i32ret(u16 a, u16 b, u16 c) {
    return (u32)a * 100000UL + (u32)b * 100UL + c;
}

__attribute__((z88dk_callee)) u16 clang_zc_noargs(void) {
    return 0x5A5Au;
}

/* Reads a stack argument after making its own call, so the cleanup has to
 * survive a nested frame. */
__attribute__((z88dk_callee)) u16 clang_zc_nested(u16 a, u16 b, u16 c) {
    u16 inner = clang_zc_3i16(a, b, c);
    return (u16)(inner + c);
}
