// RUN: rm -rf %t && split-file %s %t
// RUN: %{sdcc} -c %t/sdcc.c -o %t/sdcc.rel
// RUN: %cc %t/clang.c %t/sdcc.rel -Wl,-l,%{sdcc-lib} -o %t/prog
// RUN: %run --expect-result=0x000F %t/prog

//--- clang.c
/* Clang main calls SDCC functions with float arguments and returns in the
 * default convention. */
typedef unsigned short u16;
typedef unsigned long u32;

extern float sdcc_fadd(float a, float b);
extern float sdcc_fmul(float a, float b);
extern u16 sdcc_fcmp_gt(float a, float b);
extern u16 sdcc_f2i(float a);
extern u32 sdcc_f2l(float a);
extern float sdcc_i2f(u16 a);
extern float sdcc_fsum3(float a, float b, float c);

/* Floats are compared by bit pattern. */
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

    /* Bit 2: float and int conversions */
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

    return status;
}

//--- sdcc.c
typedef unsigned short u16;
typedef unsigned long u32;

float sdcc_fadd(float a, float b) {
    return a + b;
}

float sdcc_fmul(float a, float b) {
    return a * b;
}

u16 sdcc_fcmp_gt(float a, float b) {
    return (a > b) ? 1 : 0;
}

u16 sdcc_f2i(float a) {
    return (u16)a;
}

u32 sdcc_f2l(float a) {
    return (u32)a;
}

float sdcc_i2f(u16 a) {
    return (float)a;
}

float sdcc_fsum3(float a, float b, float c) {
    return a + b + c;
}
