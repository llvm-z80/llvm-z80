/* SDCC functions for float cross-boundary tests */
typedef unsigned short u16;
typedef unsigned long u32;

/* float add */
float sdcc_fadd(float a, float b) {
    return a + b;
}

/* float multiply */
float sdcc_fmul(float a, float b) {
    return a * b;
}

/* float compare */
u16 sdcc_fcmp_gt(float a, float b) {
    return (a > b) ? 1 : 0;
}

/* float to int conversion */
u16 sdcc_f2i(float a) {
    return (u16)a;
}

u32 sdcc_f2l(float a) {
    return (u32)a;
}

/* int to float conversion */
float sdcc_i2f(u16 a) {
    return (float)a;
}

/* float accumulate */
float sdcc_fsum3(float a, float b, float c) {
    return a + b + c;
}
