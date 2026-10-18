// RUN: rm -rf %t && split-file %s %t
// RUN: %{sdcc} -c %t/sdcc.c -o %t/sdcc.rel
// RUN: %cc %t/clang.c %t/sdcc.rel -Wl,-l,%{sdcc-lib} -o %t/prog
// RUN: %run --expect-result=0x000F %t/prog

//--- clang.c
/* Clang main passes a long or float with a zero low half on the stack to
 * SDCC __sdcccall(0), __smallc and __smallc __z88dk_callee functions. */
typedef unsigned short u16;
typedef unsigned long u32;

union FB { float f; u16 w[2]; };

#define SC   __attribute__((smallc))
#define ZC   __attribute__((z88dk_callee))
#define C0   __attribute__((sdcccall(0)))

extern u16 sdcc_zh_c0_lo(u32 a) C0;
extern u16 sdcc_zh_c0_hi(u32 a) C0;
extern u16 sdcc_zh_sc_lo(u32 a) SC;
extern u16 sdcc_zh_scc_lo(u32 a) SC ZC;
extern u16 sdcc_zh_c0_flo(float a) C0;
extern u16 sdcc_zh_sc_flo(float a) SC;

int main(void) {
    volatile u16 status = 0;

    /* Bit 0: low half zero, __sdcccall(0). */
    if (sdcc_zh_c0_lo(0x12340000UL) == 0x0000 &&
        sdcc_zh_c0_hi(0x12340000UL) == 0x1234)
        status |= (1 << 0);

    /* Bit 1: the same under __smallc, with and without __z88dk_callee. */
    if (sdcc_zh_sc_lo(0x12340000UL) == 0x0000 &&
        sdcc_zh_scc_lo(0x12340000UL) == 0x0000)
        status |= (1 << 1);

    /* Bit 2: float 2.0f (0x40000000) with a zero low half. */
    if (sdcc_zh_c0_flo(2.0f) == 0x0000 && sdcc_zh_sc_flo(2.0f) == 0x0000)
        status |= (1 << 2);

    /* Bit 3: both halves zero. */
    if (sdcc_zh_c0_lo(0UL) == 0x0000 && sdcc_zh_c0_hi(0UL) == 0x0000)
        status |= (1 << 3);

    return status;
}

//--- sdcc.c
typedef unsigned short u16;
typedef unsigned long u32;

union FB { float f; u16 w[2]; };

u16 sdcc_zh_c0_lo(u32 a) __sdcccall(0) { return (u16)a; }
u16 sdcc_zh_c0_hi(u32 a) __sdcccall(0) { return (u16)(a >> 16); }
u16 sdcc_zh_sc_lo(u32 a) __smallc { return (u16)a; }
u16 sdcc_zh_scc_lo(u32 a) __smallc __z88dk_callee { return (u16)a; }
u16 sdcc_zh_c0_flo(float a) __sdcccall(0) { union FB u; u.f = a; return u.w[0]; }
u16 sdcc_zh_sc_flo(float a) __smallc { union FB u; u.f = a; return u.w[0]; }
