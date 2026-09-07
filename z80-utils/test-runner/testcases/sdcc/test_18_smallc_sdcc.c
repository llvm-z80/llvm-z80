/* SDCC-compiled __smallc functions for ABI compatibility tests */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

/* __smallc: every argument on the stack, pushed LEFT-TO-RIGHT, caller
 * cleanup, return i8->L, i16->HL, i32->DEHL.  It differs from __sdcccall(0)
 * only in the push order, so every function here is deliberately
 * non-commutative in its arguments: a positional encoding makes a swapped or
 * reversed argument list produce a different number instead of the same one. */

u16 sdcc_sc_2i16(u16 a, u16 b) __smallc {
    return (u16)(a * 100u + b);
}

u16 sdcc_sc_3i16(u16 a, u16 b, u16 c) __smallc {
    return (u16)(a * 100u + b * 10u + c);
}

u16 sdcc_sc_4i16(u16 a, u16 b, u16 c, u16 d) __smallc {
    return (u16)(a * 1000u + b * 100u + c * 10u + d);
}

/* One argument: __smallc and __sdcccall(0) are indistinguishable here, which
 * makes this the control for the three above. */
u16 sdcc_sc_1i16(u16 a) __smallc {
    return (u16)(a + 1u);
}

/* Mixed widths: __smallc gives every argument a full two-byte slot, so the
 * i8s here sit two bytes apart, not one.  A mirrored layout has to get the
 * slot sizes right as well as the order. */
u16 sdcc_sc_i8_i16_i8(u8 a, u16 b, u8 c) __smallc {
    return (u16)(a * 1000u + b * 10u + c);
}

/* i32 return travels in DEHL under __smallc, unlike __sdcccall(1)'s HLDE. */
u32 sdcc_sc_i32(u16 a, u16 b) __smallc {
    return (u32)a * 100000UL + b;
}

/* A __smallc function calling another one, to exercise the order on a nested
 * frame rather than only at the outermost call. */
u16 sdcc_sc_nested(u16 a, u16 b, u16 c) __smallc {
    return (u16)(sdcc_sc_3i16(a, b, c) + sdcc_sc_2i16(c, a));
}
