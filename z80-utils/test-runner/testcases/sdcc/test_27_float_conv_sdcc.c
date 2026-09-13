/* SDCC float functions under the z88dk conventions.
 *
 * A float is four bytes, so it occupies the widest scalar slot each
 * convention has, and its two halves have to arrive in the right order:
 * __sdcccall(1) and the z88dk classic set disagree about which half is high.
 *
 * The constants are handed over as bit patterns and checked the same way
 * rather than compared as floats, so a failure names the half that went
 * wrong.  Neither half is zero here; that case is test 29.  Doubling is exact
 * in binary floating point, so the expected pattern is the argument with its
 * exponent bumped. */
typedef unsigned short u16;

union FB { float f; u16 w[2]; };

/* Halves of the incoming argument, so a misplaced word is visible directly. */
u16 sdcc_fl_c0_lo(float a) __sdcccall(0) { union FB u; u.f = a; return u.w[0]; }
u16 sdcc_fl_c0_hi(float a) __sdcccall(0) { union FB u; u.f = a; return u.w[1]; }
u16 sdcc_fl_sc_lo(float a) __smallc { union FB u; u.f = a; return u.w[0]; }
u16 sdcc_fl_sc_hi(float a) __smallc { union FB u; u.f = a; return u.w[1]; }
u16 sdcc_fl_zc_lo(float a) __z88dk_callee { union FB u; u.f = a; return u.w[0]; }
u16 sdcc_fl_zc_hi(float a) __z88dk_callee { union FB u; u.f = a; return u.w[1]; }
u16 sdcc_fl_c0c_lo(float a) __sdcccall(0) __z88dk_callee { union FB u; u.f = a; return u.w[0]; }
u16 sdcc_fl_scc_lo(float a) __smallc __z88dk_callee { union FB u; u.f = a; return u.w[0]; }

/* Two floats, non-commutative, to pin the order of two wide stack slots. */
u16 sdcc_fl_c0_pair(float a, float b) __sdcccall(0) {
    return (u16)((u16)(a * 100.0f) + (u16)b);
}
u16 sdcc_fl_sc_pair(float a, float b) __smallc {
    return (u16)((u16)(a * 100.0f) + (u16)b);
}
u16 sdcc_fl_scc_pair(float a, float b) __smallc __z88dk_callee {
    return (u16)((u16)(a * 100.0f) + (u16)b);
}

/* Doubling is exact, so the returned pattern is the argument's with the
 * exponent incremented, and both halves are checked. */
float sdcc_fl_c0_dbl(float a) __sdcccall(0) { return a * 2.0f; }
float sdcc_fl_sc_dbl(float a) __smallc { return a * 2.0f; }
float sdcc_fl_zc_dbl(float a) __z88dk_callee { return a * 2.0f; }
float sdcc_fl_c0c_dbl(float a) __sdcccall(0) __z88dk_callee { return a * 2.0f; }
float sdcc_fl_scc_dbl(float a) __smallc __z88dk_callee { return a * 2.0f; }

/* A narrow argument on either side of a float, where __smallc pads the
 * narrow ones to a full slot and the float still takes two. */
u16 sdcc_fl_mixed(unsigned char t, float a, u16 u) __smallc {
    return (u16)(t * 1000u + (u16)a * 10u + u);
}
