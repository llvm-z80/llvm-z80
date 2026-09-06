/* Test 12: Float cross-boundary tests */
typedef unsigned short u16;
typedef unsigned long u32;

extern float sdcc_fadd(float a, float b);
extern float sdcc_fmul(float a, float b);
extern u16 sdcc_fcmp_gt(float a, float b);
extern u16 sdcc_f2i(float a);
extern u32 sdcc_f2l(float a);
extern float sdcc_i2f(u16 a);
extern float sdcc_fsum3(float a, float b, float c);

/* Helper: compare float via bit pattern (avoid FP comparison issues) */
static u32 f2bits(float f) {
    union { float f; u32 u; } x;
    x.f = f;
    return x.u;
}

int main(void) {
    volatile u16 status = 0;

    /* Bit 0: float add and multiply */
    {
        volatile float r1 = sdcc_fadd(1.0f, 2.0f);
        volatile float r2 = sdcc_fmul(3.0f, 4.0f);
        /* 3.0f = 0x40400000, 12.0f = 0x41400000 */
        if (f2bits(r1) == 0x40400000UL && f2bits(r2) == 0x41400000UL)
            status |= (1 << 0);
    }

    /* Bit 1: float compare */
    {
        volatile u16 r1 = sdcc_fcmp_gt(3.0f, 2.0f);
        volatile u16 r2 = sdcc_fcmp_gt(1.0f, 2.0f);
        if (r1 == 1 && r2 == 0)
            status |= (1 << 1);
    }

    /* Bit 2: float ↔ int conversion */
    {
        volatile u16 r1 = sdcc_f2i(42.9f);
        volatile u32 r2 = sdcc_f2l(100000.0f);
        volatile float r3 = sdcc_i2f(1000);
        /* 1000.0f = 0x447A0000 */
        if (r1 == 42 && r2 == 100000UL && f2bits(r3) == 0x447A0000UL)
            status |= (1 << 2);
    }

    /* Bit 3: float with 3 args (stack usage) */
    {
        volatile float r = sdcc_fsum3(1.0f, 2.0f, 3.0f);
        /* 6.0f = 0x40C00000 */
        if (f2bits(r) == 0x40C00000UL)
            status |= (1 << 3);
    }

    return status; /* expect 0x000F */
}
