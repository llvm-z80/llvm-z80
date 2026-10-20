// RUN: rm -rf %t && split-file %s %t
// RUN: %{sdcc} -c %t/sdcc.c -o %t/sdcc.rel
// RUN: %cc %t/clang.c %t/sdcc.rel -Wl,-l,%{sdcc-lib} -o %t/prog
// RUN: %run --expect-result=0x003F %t/prog

//--- clang.c
/* SDCC main calls Clang __sdcccall(0), __smallc and __z88dk_callee
 * functions, alone and combined, with float arguments and returns. Clang is
 * the callee, so Clang has to match the SDCC layout of four-byte arguments and
 * returns. */
typedef unsigned char u8;
typedef unsigned short u16;

union FB { float f; u16 w[2]; };

#define SC   __attribute__((smallc))
#define ZC   __attribute__((z88dk_callee))
#define C0   __attribute__((sdcccall(0)))

C0 u16 clang_fl_c0_lo(float a)  { union FB u; u.f = a; return u.w[0]; }
C0 u16 clang_fl_c0_hi(float a)  { union FB u; u.f = a; return u.w[1]; }
SC u16 clang_fl_sc_lo(float a)  { union FB u; u.f = a; return u.w[0]; }
SC u16 clang_fl_sc_hi(float a)  { union FB u; u.f = a; return u.w[1]; }
ZC u16 clang_fl_zc_lo(float a)  { union FB u; u.f = a; return u.w[0]; }
ZC u16 clang_fl_zc_hi(float a)  { union FB u; u.f = a; return u.w[1]; }
C0 ZC u16 clang_fl_c0c_lo(float a) { union FB u; u.f = a; return u.w[0]; }
SC ZC u16 clang_fl_scc_lo(float a) { union FB u; u.f = a; return u.w[0]; }

C0 u16 clang_fl_c0_pair(float a, float b) { return (u16)((u16)(a * 100.0f) + (u16)b); }
SC u16 clang_fl_sc_pair(float a, float b) { return (u16)((u16)(a * 100.0f) + (u16)b); }
SC ZC u16 clang_fl_scc_pair(float a, float b) { return (u16)((u16)(a * 100.0f) + (u16)b); }

/* Doubling is exact, so the returned pattern is the argument pattern with the
 * exponent raised by one. */
C0 float clang_fl_c0_dbl(float a)    { return a * 2.0f; }
SC float clang_fl_sc_dbl(float a)    { return a * 2.0f; }
ZC float clang_fl_zc_dbl(float a)    { return a * 2.0f; }
C0 ZC float clang_fl_c0c_dbl(float a) { return a * 2.0f; }
SC ZC float clang_fl_scc_dbl(float a) { return a * 2.0f; }

SC u16 clang_fl_mixed(u8 t, float a, u16 u) {
    return (u16)(t * 1000u + (u16)a * 10u + u);
}

//--- sdcc.c
typedef unsigned char u8;
typedef unsigned short u16;

union FB { float f; u16 w[2]; };

extern u16 clang_fl_c0_lo(float a) __sdcccall(0);
extern u16 clang_fl_c0_hi(float a) __sdcccall(0);
extern u16 clang_fl_sc_lo(float a) __smallc;
extern u16 clang_fl_sc_hi(float a) __smallc;
extern u16 clang_fl_zc_lo(float a) __z88dk_callee;
extern u16 clang_fl_zc_hi(float a) __z88dk_callee;
extern u16 clang_fl_c0c_lo(float a) __sdcccall(0) __z88dk_callee;
extern u16 clang_fl_scc_lo(float a) __smallc __z88dk_callee;

extern u16 clang_fl_c0_pair(float a, float b) __sdcccall(0);
extern u16 clang_fl_sc_pair(float a, float b) __smallc;
extern u16 clang_fl_scc_pair(float a, float b) __smallc __z88dk_callee;

extern float clang_fl_c0_dbl(float a) __sdcccall(0);
extern float clang_fl_sc_dbl(float a) __smallc;
extern float clang_fl_zc_dbl(float a) __z88dk_callee;
extern float clang_fl_c0c_dbl(float a) __sdcccall(0) __z88dk_callee;
extern float clang_fl_scc_dbl(float a) __smallc __z88dk_callee;

extern u16 clang_fl_mixed(u8 t, float a, u16 u) __smallc;

/* 0x3FA00001 is 1.25f with the lowest mantissa bit set. Doubling is exact and
 * gives 0x40200001. */
#define ARG_LO 0x0001
#define ARG_HI 0x3FA0
#define DBL_LO 0x0001
#define DBL_HI 0x4020

static u8 doubled_ok(float r) {
    union FB u;
    u.f = r;
    return (u8)(u.w[0] == DBL_LO && u.w[1] == DBL_HI);
}

int main(void) {
    u16 status = 0;
    union FB in;
    float a;

    in.w[0] = ARG_LO;
    in.w[1] = ARG_HI;
    a = in.f;

    /* Bit 0: both halves of a float argument, right-to-left conventions. */
    if (clang_fl_c0_lo(a) == ARG_LO && clang_fl_c0_hi(a) == ARG_HI &&
        clang_fl_zc_lo(a) == ARG_LO && clang_fl_zc_hi(a) == ARG_HI &&
        clang_fl_c0c_lo(a) == ARG_LO)
        status |= (1 << 0);

    /* Bit 1: the same under the left-to-right order. */
    if (clang_fl_sc_lo(a) == ARG_LO && clang_fl_sc_hi(a) == ARG_HI &&
        clang_fl_scc_lo(a) == ARG_LO)
        status |= (1 << 1);

    /* Bit 2: two float arguments, non-commutative. */
    if (clang_fl_c0_pair(2.0f, 5.0f) == 205 &&
        clang_fl_sc_pair(2.0f, 5.0f) == 205 &&
        clang_fl_scc_pair(2.0f, 5.0f) == 205)
        status |= (1 << 2);

    /* Bit 3: float returns under the right-to-left conventions. */
    if (doubled_ok(clang_fl_c0_dbl(a)) && doubled_ok(clang_fl_zc_dbl(a)) &&
        doubled_ok(clang_fl_c0c_dbl(a)))
        status |= (1 << 3);

    /* Bit 4: float returns under the left-to-right order. */
    if (doubled_ok(clang_fl_sc_dbl(a)) && doubled_ok(clang_fl_scc_dbl(a)))
        status |= (1 << 4);

    /* Bit 5: a narrow argument on either side of a float. */
    if (clang_fl_mixed(9, 87.0f, 6) == 9876)
        status |= (1 << 5);

    return status;
}
