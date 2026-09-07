/* SDCC-compiled __z88dk_callee combinations for ABI compatibility tests */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

/* The __z88dk_callee modifier applies to each argument-passing base in turn,
 * and the three results are genuinely different conventions:
 *
 *   __sdcccall(0) __z88dk_callee  every argument on the stack, right-to-left
 *   __smallc      __z88dk_callee  every argument on the stack, left-to-right
 *
 * The second is what the z88dk classic C library uses for its `_callee` entry
 * points.  Both clean up in the callee, unlike their unmodified forms, and
 * both return in DEHL rather than HLDE. */

u16 sdcc_c0c_3i16(u16 a, u16 b, u16 c) __sdcccall(0) __z88dk_callee {
    return (u16)(a * 100u + b * 10u + c);
}

u16 sdcc_c0c_4i16(u16 a, u16 b, u16 c, u16 d) __sdcccall(0) __z88dk_callee {
    return (u16)(a * 1000u + b * 100u + c * 10u + d);
}

u32 sdcc_c0c_i32(u16 a, u16 b) __sdcccall(0) __z88dk_callee {
    return (u32)a * 100000UL + b;
}

u16 sdcc_scc_3i16(u16 a, u16 b, u16 c) __smallc __z88dk_callee {
    return (u16)(a * 100u + b * 10u + c);
}

u16 sdcc_scc_4i16(u16 a, u16 b, u16 c, u16 d) __smallc __z88dk_callee {
    return (u16)(a * 1000u + b * 100u + c * 10u + d);
}

u16 sdcc_scc_i8_i16_i8(u8 a, u16 b, u8 c) __smallc __z88dk_callee {
    return (u16)(a * 1000u + b * 10u + c);
}

u32 sdcc_scc_i32(u16 a, u16 b) __smallc __z88dk_callee {
    return (u32)a * 100000UL + b;
}
