/* SDCC-compiled functions for return value tests */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

/* Return i8 from i16 input */
u8 sdcc_ret_u8(u16 a) {
    return (u8)(a & 0xFF);
}

/* Return i16 from two i8 inputs */
u16 sdcc_ret_u16(u8 hi, u8 lo) {
    return ((u16)hi << 8) | lo;
}

/* Return i32 from two i16 inputs */
u32 sdcc_ret_u32(u16 hi, u16 lo) {
    return ((u32)hi << 16) | lo;
}

/* Return i32 from i32 + i32 (2nd on stack) */
u32 sdcc_add_u32(u32 a, u32 b) {
    return a + b;
}

/* Return i8 from three i8 (stack involved) */
u8 sdcc_max3_u8(u8 a, u8 b, u8 c) {
    u8 m = a;
    if (b > m) m = b;
    if (c > m) m = c;
    return m;
}
