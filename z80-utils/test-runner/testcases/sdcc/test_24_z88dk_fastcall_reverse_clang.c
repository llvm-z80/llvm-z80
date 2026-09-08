/* Clang-compiled __z88dk_fastcall functions called from SDCC main (reverse).
 * Clang is the CALLEE, so it has to read the single argument out of L/HL/DEHL
 * rather than out of its own default registers.  For an i16 those happen to
 * coincide, which is exactly why the i8 and i32 cases carry the test.
 * Z80 only; see the paired main. */
/* SKIP-IF: sm83 */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

__attribute__((z88dk_fastcall)) u8 clang_fc_i8(u8 a) {
    return (u8)(a + 1u);
}

__attribute__((z88dk_fastcall)) u16 clang_fc_i16(u16 a) {
    return (u16)(a + 1u);
}

__attribute__((z88dk_fastcall)) u32 clang_fc_i32(u32 a) {
    return a + 1UL;
}

/* i8 in, i16 out: the argument is only L and the result is all of HL. */
__attribute__((z88dk_fastcall)) u16 clang_fc_i8_widen(u8 a) {
    return (u16)(a * 100u);
}

/* i32 in, i8 out: only the high byte of DE survives into L. */
__attribute__((z88dk_fastcall)) u8 clang_fc_i32_narrow(u32 a) {
    return (u8)(a >> 24);
}
