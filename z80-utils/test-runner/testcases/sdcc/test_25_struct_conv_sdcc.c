/* SDCC functions taking and returning structs under the z88dk conventions.
 *
 * Structs reach the two ends of the ABI that the scalar tests do not: a
 * by-value argument becomes raw stack bytes rather than a slot per value, and
 * a return too wide for registers travels through a hidden pointer.  That
 * pointer is the case worth pinning, because __smallc reverses the DECLARED
 * arguments while leaving the hidden one right above the return address.
 * Mirroring it along with the others silently hands the callee an argument as
 * its return slot. */
typedef unsigned char u8;
typedef unsigned short u16;

typedef struct { u16 x, y; } P16;
typedef struct { u8 a, b, c; } S3;
typedef struct { u16 a, b, c; } Big;

/* By-value arguments. */
/* SDCC cannot generate a __smallc CALL with a struct argument (it fails with
 * an internal error), so those live on the clang side only, below. */

u16 sdcc_st_c0_val(P16 p, u16 t) __sdcccall(0) {
    return (u16)(p.x * 100u + p.y * 10u + t);
}

u16 sdcc_st_c0_odd(S3 s, u8 t) __sdcccall(0) {
    return (u16)(s.a * 1000u + s.b * 100u + s.c * 10u + t);
}

u16 sdcc_st_zc_val(P16 p, u16 t) __z88dk_callee {
    return (u16)(p.x * 100u + p.y * 10u + t);
}

u16 sdcc_st_c0c_val(P16 p, u16 t) __sdcccall(0) __z88dk_callee {
    return (u16)(p.x * 100u + p.y * 10u + t);
}

/* Struct returns through a hidden pointer. */

Big sdcc_st_c0_ret(u16 a, u16 b) __sdcccall(0) {
    Big r; r.a = a; r.b = b; r.c = (u16)(a + b); return r;
}

Big sdcc_st_sc_ret(u16 a, u16 b) __smallc {
    Big r; r.a = a; r.b = b; r.c = (u16)(a + b); return r;
}

Big sdcc_st_zc_ret(u16 a, u16 b) __z88dk_callee {
    Big r; r.a = a; r.b = b; r.c = (u16)(a + b); return r;
}

Big sdcc_st_c0c_ret(u16 a, u16 b) __sdcccall(0) __z88dk_callee {
    Big r; r.a = a; r.b = b; r.c = (u16)(a + b); return r;
}

Big sdcc_st_scc_ret(u16 a, u16 b) __smallc __z88dk_callee {
    Big r; r.a = a; r.b = b; r.c = (u16)(a + b); return r;
}

/* Three declared arguments, so the mirrored region is wide enough that a
 * hidden pointer folded into it would land on a real argument. */
Big sdcc_st_sc_ret3(u16 a, u16 b, u16 c) __smallc {
    Big r; r.a = a; r.b = b; r.c = c; return r;
}

Big sdcc_st_scc_ret3(u16 a, u16 b, u16 c) __smallc __z88dk_callee {
    Big r; r.a = a; r.b = b; r.c = c; return r;
}

/* By-value under __smallc, callee side only. */

u16 sdcc_st_sc_val(P16 p, u16 t) __smallc {
    return (u16)(p.x * 100u + p.y * 10u + t);
}

u16 sdcc_st_scc_val(P16 p, u16 t) __smallc __z88dk_callee {
    return (u16)(p.x * 100u + p.y * 10u + t);
}
