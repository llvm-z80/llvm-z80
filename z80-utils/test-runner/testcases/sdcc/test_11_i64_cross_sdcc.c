/* SDCC functions for i64 cross-boundary tests */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;
typedef unsigned long long u64;

/* i64 add (sret pattern - result returned via hidden pointer) */
u64 sdcc_add64(u64 a, u64 b) {
    return a + b;
}

/* i64 from two i32 halves */
u64 sdcc_combine32(u32 hi, u32 lo) {
    return ((u64)hi << 32) | lo;
}

/* i64 decompose to i32 */
u32 sdcc_hi32(u64 val) {
    return (u32)(val >> 32);
}

u32 sdcc_lo32(u64 val) {
    return (u32)(val & 0xFFFFFFFFUL);
}

/* i64 shift */
u64 sdcc_shl64(u64 a, u8 shift) {
    return a << shift;
}

/* i64 compare and return */
u64 sdcc_max64(u64 a, u64 b) {
    return (a > b) ? a : b;
}
