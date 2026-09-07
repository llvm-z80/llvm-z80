/* Test 29: a stack-passed 32-bit argument whose low half is zero.  SDCC
 * materialises that half from the flags register rather than loading it, so
 * the two compilers have to agree on a slot that is never written by an
 * ordinary load.  Covers both long and float under each stack convention.
 * expect 0x000F */
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

    /* Bit 0: low half zero, right-to-left base. */
    if (sdcc_zh_c0_lo(0x12340000UL) == 0x0000 &&
        sdcc_zh_c0_hi(0x12340000UL) == 0x1234)
        status |= (1 << 0);

    /* Bit 1: the same under the left-to-right order, with and without callee
     * cleanup. */
    if (sdcc_zh_sc_lo(0x12340000UL) == 0x0000 &&
        sdcc_zh_scc_lo(0x12340000UL) == 0x0000)
        status |= (1 << 1);

    /* Bit 2: a float with a zero low half -- 2.0f is 0x40000000. */
    if (sdcc_zh_c0_flo(2.0f) == 0x0000 && sdcc_zh_sc_flo(2.0f) == 0x0000)
        status |= (1 << 2);

    /* Bit 3: both halves zero. */
    if (sdcc_zh_c0_lo(0UL) == 0x0000 && sdcc_zh_c0_hi(0UL) == 0x0000)
        status |= (1 << 3);

    return status; /* expect 0x000F */
}
