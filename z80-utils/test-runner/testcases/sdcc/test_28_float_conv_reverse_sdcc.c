/* Test 28: floats across the z88dk conventions, reverse - SDCC main calls
 * Clang.
 *
 * Values travel as bit patterns rather than as compared floats, so a failure
 * names the half that went wrong.  Neither half is zero here; that case is
 * test 29.
 * expect 0x003F */
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

/* 0x3FA00001 == 1.25f with the lowest mantissa bit set; doubling gives
 * 0x40200001 exactly. */
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

    /* Bit 0: both halves of a float argument on the right-to-left bases. */
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

    /* Bit 3: float returns on the right-to-left bases. */
    if (doubled_ok(clang_fl_c0_dbl(a)) && doubled_ok(clang_fl_zc_dbl(a)) &&
        doubled_ok(clang_fl_c0c_dbl(a)))
        status |= (1 << 3);

    /* Bit 4: float returns under the left-to-right order. */
    if (doubled_ok(clang_fl_sc_dbl(a)) && doubled_ok(clang_fl_scc_dbl(a)))
        status |= (1 << 4);

    /* Bit 5: a narrow argument on either side of a float. */
    if (clang_fl_mixed(9, 87.0f, 6) == 9876)
        status |= (1 << 5);

    return status; /* expect 0x003F */
}
