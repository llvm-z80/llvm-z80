/* Clang functions taking and returning structs, called from SDCC main.
 * Clang is the CALLEE, so it is the side that has to find a by-value struct
 * and a hidden return pointer where SDCC put them. */
typedef unsigned char u8;
typedef unsigned short u16;

typedef struct { u16 x, y; } P16;
typedef struct { u8 a, b, c; } S3;
typedef struct { u16 a, b, c; } Big;

#define SC   __attribute__((smallc))
#define ZC   __attribute__((z88dk_callee))
#define C0   __attribute__((sdcccall(0)))

C0 u16 clang_st_c0_val(P16 p, u16 t) {
    return (u16)(p.x * 100u + p.y * 10u + t);
}

C0 u16 clang_st_c0_odd(S3 s, u8 t) {
    return (u16)(s.a * 1000u + s.b * 100u + s.c * 10u + t);
}

ZC u16 clang_st_zc_val(P16 p, u16 t) {
    return (u16)(p.x * 100u + p.y * 10u + t);
}

C0 ZC u16 clang_st_c0c_val(P16 p, u16 t) {
    return (u16)(p.x * 100u + p.y * 10u + t);
}

C0 Big clang_st_c0_ret(u16 a, u16 b) {
    Big r; r.a = a; r.b = b; r.c = (u16)(a + b); return r;
}

SC Big clang_st_sc_ret(u16 a, u16 b) {
    Big r; r.a = a; r.b = b; r.c = (u16)(a + b); return r;
}

ZC Big clang_st_zc_ret(u16 a, u16 b) {
    Big r; r.a = a; r.b = b; r.c = (u16)(a + b); return r;
}

C0 ZC Big clang_st_c0c_ret(u16 a, u16 b) {
    Big r; r.a = a; r.b = b; r.c = (u16)(a + b); return r;
}

SC ZC Big clang_st_scc_ret(u16 a, u16 b) {
    Big r; r.a = a; r.b = b; r.c = (u16)(a + b); return r;
}

/* Three declared arguments, so the mirrored region is wide enough that a
 * hidden pointer folded into it would collide with a real argument. */
SC Big clang_st_sc_ret3(u16 a, u16 b, u16 c) {
    Big r; r.a = a; r.b = b; r.c = c; return r;
}

SC ZC Big clang_st_scc_ret3(u16 a, u16 b, u16 c) {
    Big r; r.a = a; r.b = b; r.c = c; return r;
}
