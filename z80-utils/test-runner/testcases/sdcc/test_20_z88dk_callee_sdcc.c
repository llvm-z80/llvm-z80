/* SDCC-compiled __z88dk_callee functions for ABI compatibility tests */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

/* __z88dk_callee on its own is a MODIFIER over whichever __sdcccall level is
 * in effect, not a convention of its own: the argument passing and return
 * registers stay __sdcccall(1)'s, and only stack cleanup moves to the callee.
 * The first two arguments therefore travel in registers and only the third
 * onwards is pushed, which is what separates this from
 * __sdcccall(0) __z88dk_callee (test 22).
 *
 * The invariant under test is EXACTLY-ONCE cleanup.  A caller that also pops
 * leaves the stack pointer too high and a caller that never pops leaks, so
 * both failures show up as a corrupted frame after a few calls rather than as
 * a single wrong return value.  Every function is non-commutative so a
 * mis-assigned argument register is visible too. */

u16 sdcc_zc_2i16(u16 a, u16 b) __z88dk_callee {
    return (u16)(a * 100u + b);
}

u16 sdcc_zc_3i16(u16 a, u16 b, u16 c) __z88dk_callee {
    return (u16)(a * 100u + b * 10u + c);
}

u16 sdcc_zc_5i16(u16 a, u16 b, u16 c, u16 d, u16 e) __z88dk_callee {
    return (u16)(a * 10000u + b * 1000u + c * 100u + d * 10u + e);
}

u16 sdcc_zc_i8_i16(u8 a, u16 b) __z88dk_callee {
    return (u16)(a * 1000u + b);
}

/* A 32-bit return is where the modifier earns its keep: plain __sdcccall(1)
 * hands cleanup back to the caller once the return exceeds 16 bits, and
 * __z88dk_callee overrides that.  The value comes back in HLDE, not DEHL. */
u32 sdcc_zc_i32ret(u16 a, u16 b, u16 c) __z88dk_callee {
    return (u32)a * 100000UL + (u32)b * 100UL + c;
}

/* No stack argument at all - nothing to clean, and no spurious pop. */
u16 sdcc_zc_noargs(void) __z88dk_callee {
    return 0x5A5Au;
}

void sdcc_zc_void(u16 a, u16 b, u16 c, u16 *out) __z88dk_callee {
    *out = (u16)(a * 100u + b * 10u + c);
}
