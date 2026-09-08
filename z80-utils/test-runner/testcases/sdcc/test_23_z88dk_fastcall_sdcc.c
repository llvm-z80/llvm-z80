/* SKIP-IF: sm83 */
/* SDCC-compiled __z88dk_fastcall functions for ABI compatibility tests.
 *
 * z88dk fastcall is single-argument by construction and passes it in the
 * classic return registers: i8 in L, i16 in HL, i32 in DEHL (DE high).
 * Neither SDCC nor clang accepts the keyword for SM83, whose ABI for it has
 * never been specified, so the pair is skipped on that target. */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

u8 sdcc_fc_i8(u8 a) __z88dk_fastcall {
    return (u8)(a + 1u);
}

u16 sdcc_fc_i16(u16 a) __z88dk_fastcall {
    return (u16)(a + 1u);
}

u32 sdcc_fc_i32(u32 a) __z88dk_fastcall {
    return a + 1UL;
}

/* Widening: the argument arrives in L, and the high byte of HL is not part of
 * the value, so a callee that read all of HL would pick up garbage. */
u16 sdcc_fc_i8_widen(u8 a) __z88dk_fastcall {
    return (u16)(a * 100u);
}

/* Narrowing on the way out: the result is an i8 in L even though the work is
 * done on a wider value. */
u8 sdcc_fc_i32_narrow(u32 a) __z88dk_fastcall {
    return (u8)(a >> 24);
}

/* A fastcall function that itself calls one, so the single argument register
 * has to survive being reloaded for the inner call. */
u16 sdcc_fc_chain(u16 a) __z88dk_fastcall {
    return (u16)(sdcc_fc_i16(a) * 2u);
}
