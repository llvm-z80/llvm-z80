// RUN: rm -rf %t && split-file %s %t
// RUN: %{sdcc} -c %t/sdcc.c -o %t/sdcc.rel
// RUN: %cc %t/clang.c %t/sdcc.rel -Wl,-l,%{sdcc-lib} -o %t/prog
// RUN: %run --expect-result=0x003F %t/prog

//--- clang.c
/* Clang main calls SDCC __sdcccall(0), __smallc and __z88dk_callee
 * functions, alone and combined, with float arguments and returns. */
typedef unsigned char u8;
typedef unsigned short u16;

union FB { float f; u16 w[2]; };

#define SC   __attribute__((smallc))
#define ZC   __attribute__((z88dk_callee))
#define C0   __attribute__((sdcccall(0)))

extern u16 sdcc_fl_c0_lo(float a) C0;
extern u16 sdcc_fl_c0_hi(float a) C0;
extern u16 sdcc_fl_sc_lo(float a) SC;
extern u16 sdcc_fl_sc_hi(float a) SC;
extern u16 sdcc_fl_zc_lo(float a) ZC;
extern u16 sdcc_fl_zc_hi(float a) ZC;
extern u16 sdcc_fl_c0c_lo(float a) C0 ZC;
extern u16 sdcc_fl_scc_lo(float a) SC ZC;

extern u16 sdcc_fl_c0_pair(float a, float b) C0;
extern u16 sdcc_fl_sc_pair(float a, float b) SC;
extern u16 sdcc_fl_scc_pair(float a, float b) SC ZC;

extern float sdcc_fl_c0_dbl(float a) C0;
extern float sdcc_fl_sc_dbl(float a) SC;
extern float sdcc_fl_zc_dbl(float a) ZC;
extern float sdcc_fl_c0c_dbl(float a) C0 ZC;
extern float sdcc_fl_scc_dbl(float a) SC ZC;

extern u16 sdcc_fl_mixed(u8 t, float a, u16 u) SC;

/* 0x3FA00001 is 1.25f with the lowest mantissa bit set, so neither half is
 * zero. Doubling is exact and gives 0x40200001. */
#define ARG_LO 0x0001
#define ARG_HI 0x3FA0
#define DBL_LO 0x0001
#define DBL_HI 0x4020

static float arg_value(void) {
    union FB u;
    u.w[0] = ARG_LO;
    u.w[1] = ARG_HI;
    return u.f;
}

static int doubled_ok(float r) {
    union FB u;
    u.f = r;
    return u.w[0] == DBL_LO && u.w[1] == DBL_HI;
}

int main(void) {
    volatile u16 status = 0;
    float a = arg_value();

    /* Bit 0: both halves of a float argument, right-to-left conventions. */
    {
        if (sdcc_fl_c0_lo(a) == ARG_LO && sdcc_fl_c0_hi(a) == ARG_HI &&
            sdcc_fl_zc_lo(a) == ARG_LO && sdcc_fl_zc_hi(a) == ARG_HI &&
            sdcc_fl_c0c_lo(a) == ARG_LO)
            status |= (1 << 0);
    }

    /* Bit 1: the same under the left-to-right order. */
    {
        if (sdcc_fl_sc_lo(a) == ARG_LO && sdcc_fl_sc_hi(a) == ARG_HI &&
            sdcc_fl_scc_lo(a) == ARG_LO)
            status |= (1 << 1);
    }

    /* Bit 2: two float arguments in a non-commutative function. */
    {
        if (sdcc_fl_c0_pair(2.0f, 5.0f) == 205 &&
            sdcc_fl_sc_pair(2.0f, 5.0f) == 205 &&
            sdcc_fl_scc_pair(2.0f, 5.0f) == 205)
            status |= (1 << 2);
    }

    /* Bit 3: float returns under the right-to-left conventions. */
    {
        if (doubled_ok(sdcc_fl_c0_dbl(a)) && doubled_ok(sdcc_fl_zc_dbl(a)) &&
            doubled_ok(sdcc_fl_c0c_dbl(a)))
            status |= (1 << 3);
    }

    /* Bit 4: float returns under the left-to-right order. */
    {
        if (doubled_ok(sdcc_fl_sc_dbl(a)) && doubled_ok(sdcc_fl_scc_dbl(a)))
            status |= (1 << 4);
    }

    /* Bit 5: a narrow argument on either side of a float. */
    {
        if (sdcc_fl_mixed(9, 87.0f, 6) == 9876)
            status |= (1 << 5);
    }

    return status;
}

//--- sdcc.c
typedef unsigned short u16;

union FB { float f; u16 w[2]; };

/* Halves of the incoming argument. */
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

/* Doubling is exact, so both halves of the result are checked. */
float sdcc_fl_c0_dbl(float a) __sdcccall(0) { return a * 2.0f; }
float sdcc_fl_sc_dbl(float a) __smallc { return a * 2.0f; }
float sdcc_fl_zc_dbl(float a) __z88dk_callee { return a * 2.0f; }
float sdcc_fl_c0c_dbl(float a) __sdcccall(0) __z88dk_callee { return a * 2.0f; }
float sdcc_fl_scc_dbl(float a) __smallc __z88dk_callee { return a * 2.0f; }

/* A narrow argument on either side of a float. __smallc pads each narrow
 * argument to a full slot, and the float takes two slots. */
u16 sdcc_fl_mixed(unsigned char t, float a, u16 u) __smallc {
    return (u16)(t * 1000u + (u16)a * 10u + u);
}
