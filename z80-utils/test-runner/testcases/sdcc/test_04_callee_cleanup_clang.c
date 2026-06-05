/* Test 04: Callee-cleanup stress - Clang main calls SDCC functions */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

extern u16 sdcc_cc_add3(u16 a, u16 b, u16 c);
extern u16 sdcc_cc_add4(u16 a, u16 b, u16 c, u16 d);
extern u32 sdcc_nc_mul_add(u16 a, u16 b, u16 c);
extern void sdcc_void_sum(u16 a, u16 b, u16 c, u16 *out);
extern u16 sdcc_chain(u16 a, u16 b, u16 c);

int main(void) {
    volatile u16 status = 0;

    /* Bit 0: callee-cleanup i16 return */
    {
        volatile u16 r1 = sdcc_cc_add3(100, 200, 300);
        volatile u16 r2 = sdcc_cc_add4(1, 2, 3, 4);
        if (r1 == 600 && r2 == 10)
            status |= (1 << 0);
    }

    /* Bit 1: caller-cleanup i32 return (Z80), callee-cleanup (SM83) */
    {
        volatile u32 r = sdcc_nc_mul_add(100, 200, 50);
        if (r == 20050UL)
            status |= (1 << 1);
    }

    /* Bit 2: interleaved callee/caller cleanup calls */
    {
        volatile u16 r1 = sdcc_cc_add3(10, 20, 30);   /* callee-cleanup */
        volatile u32 r2 = sdcc_nc_mul_add(10, 10, 5);  /* caller-cleanup(Z80) */
        volatile u16 r3 = sdcc_cc_add3(1, 2, 3);       /* callee-cleanup */
        volatile u32 r4 = sdcc_nc_mul_add(5, 5, 1);    /* caller-cleanup(Z80) */
        if (r1 == 60 && r2 == 105UL && r3 == 6 && r4 == 26UL)
            status |= (1 << 2);
    }

    /* Bit 3: void + pointer, chain call */
    {
        volatile u16 out = 0;
        sdcc_void_sum(10, 20, 30, (u16 *)&out);
        volatile u16 r = sdcc_chain(10, 20, 30);
        if (out == 60 && r == 120)
            status |= (1 << 3);
    }

    return status; /* expect 0x000F */
}
