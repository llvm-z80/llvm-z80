/* SDCC-compiled functions for stack argument tests */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

/* 3 x i8: 3rd on stack */
u8 sdcc_3i8(u8 a, u8 b, u8 c) {
    return a + b + c;
}

/* 3 x i16: 3rd on stack, callee-cleanup (ret i16) */
u16 sdcc_3i16(u16 a, u16 b, u16 c) {
    return a + b + c;
}

/* 4 x i16: 3rd,4th on stack */
u16 sdcc_4i16(u16 a, u16 b, u16 c, u16 d) {
    return a + b + c + d;
}

/* i32 + i16: i32 fills registers, i16 on stack */
u16 sdcc_i32_i16(u32 a, u16 b) {
    return (u16)(a & 0xFFFF) + b;
}

/* 5 x i8: heavy stack usage (3 i8 on stack, packed bytes) */
u8 sdcc_5i8(u8 a, u8 b, u8 c, u8 d, u8 e) {
    return a + b + c + d + e;
}

/* 5 x i16: heavy stack usage */
u16 sdcc_5i16(u16 a, u16 b, u16 c, u16 d, u16 e) {
    return a + b + c + d + e;
}
