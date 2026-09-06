/* SDCC functions for complex i32 argument/return tests */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

/* i32 + i32 on stack → i32 return */
u32 sdcc_add32(u32 a, u32 b) {
    return a + b;
}

/* i32 arithmetic chain */
u32 sdcc_mul32_add(u32 a, u32 b, u16 c) {
    return a * b + c;
}

/* i32 shift operations */
u32 sdcc_shl32(u32 a, u8 shift) {
    return a << shift;
}

/* Two i32 returns in sequence - tests register restore */
u32 sdcc_max32(u32 a, u32 b) {
    return (a > b) ? a : b;
}

/* i32 from multiple i16 args */
u32 sdcc_combine16(u16 hi, u16 lo) {
    return ((u32)hi << 16) | lo;
}

/* i32 decompose */
u16 sdcc_hi16(u32 val) {
    return (u16)(val >> 16);
}

u16 sdcc_lo16(u32 val) {
    return (u16)(val & 0xFFFF);
}
