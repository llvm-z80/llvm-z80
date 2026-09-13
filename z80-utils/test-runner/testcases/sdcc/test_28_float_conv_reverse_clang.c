/* Clang float functions under the z88dk conventions, called from SDCC main.
 * Clang is the CALLEE, so it has to find four-byte arguments where SDCC laid
 * them out and return the halves in the registers the base dictates. */
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

/* Doubling is exact, so the returned pattern is the argument's with the
 * exponent incremented. */
C0 float clang_fl_c0_dbl(float a)    { return a * 2.0f; }
SC float clang_fl_sc_dbl(float a)    { return a * 2.0f; }
ZC float clang_fl_zc_dbl(float a)    { return a * 2.0f; }
C0 ZC float clang_fl_c0c_dbl(float a) { return a * 2.0f; }
SC ZC float clang_fl_scc_dbl(float a) { return a * 2.0f; }

SC u16 clang_fl_mixed(u8 t, float a, u16 u) {
    return (u16)(t * 1000u + (u16)a * 10u + u);
}
