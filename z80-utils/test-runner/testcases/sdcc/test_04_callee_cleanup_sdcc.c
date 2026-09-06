/* SDCC-compiled functions for callee-cleanup stress test */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

/* Callee-cleanup: ret i16 with stack args */
u16 sdcc_cc_add3(u16 a, u16 b, u16 c) {
    return a + b + c;
}

/* Callee-cleanup: ret i16 with 4 args */
u16 sdcc_cc_add4(u16 a, u16 b, u16 c, u16 d) {
    return a + b + c + d;
}

/* Caller-cleanup on Z80 (ret i32 > 16 bits), callee on SM83 */
u32 sdcc_nc_mul_add(u16 a, u16 b, u16 c) {
    return (u32)a * b + c;
}

/* Void return with pointer: callee-cleanup */
void sdcc_void_sum(u16 a, u16 b, u16 c, u16 *out) {
    *out = a + b + c;
}

/* Chain: callee calls another callee-cleanup func */
u16 sdcc_chain(u16 a, u16 b, u16 c) {
    u16 tmp = sdcc_cc_add3(a, b, c);
    return tmp * 2;
}
