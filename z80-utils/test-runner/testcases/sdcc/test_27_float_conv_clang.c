/* Test 27: floats across the z88dk conventions - Clang main calls SDCC.
 * expect 0x003F */
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

/* 0x3FA00001 == 1.25f with the lowest mantissa bit set, so neither half is
 * zero and doubling stays exact: 0x40200001. */
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

    /* Bit 0: both halves of a float argument on the right-to-left bases. */
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

    /* Bit 2: two float arguments, non-commutative, so a swapped pair of
     * four-byte slots gives 502 instead of 205. */
    {
        if (sdcc_fl_c0_pair(2.0f, 5.0f) == 205 &&
            sdcc_fl_sc_pair(2.0f, 5.0f) == 205 &&
            sdcc_fl_scc_pair(2.0f, 5.0f) == 205)
            status |= (1 << 2);
    }

    /* Bit 3: float returns on the right-to-left bases.  __sdcccall(1) and the
     * z88dk classic set put the halves in opposite registers, so a convention
     * taking its return registers from the wrong base swaps the words. */
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

    return status; /* expect 0x003F */
}
