/* SDCC functions for mixed stack argument tests */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

/* Mixed i8+i16 on stack: i8,i8,i16,i8 → 2 regs + 1 i16 + 1 i8 on stack */
u16 sdcc_mix_i8i8i16i8(u8 a, u8 b, u16 c, u8 d) {
    return a + b + c + d;
}

/* Mixed i16+i8 on stack: i16,i16,i8,i16 → 2 regs + 1 i8 + 1 i16 on stack */
u16 sdcc_mix_i16i16i8i16(u16 a, u16 b, u8 c, u16 d) {
    return a + b + c + d;
}

/* i32 + multiple stack args: i32,i16,i8 → i32 fills regs, i16+i8 on stack */
u16 sdcc_i32_i16_i8(u32 a, u16 b, u8 c) {
    return (u16)(a & 0xFF) + b + c;
}

/* 6 x i16 → 2 regs + 4 on stack */
u16 sdcc_6i16(u16 a, u16 b, u16 c, u16 d, u16 e, u16 f) {
    return a + b + c + d + e + f;
}

/* 8 x i8 → 2 regs + 6 on stack (packed i8) */
u8 sdcc_8i8(u8 a, u8 b, u8 c, u8 d, u8 e, u8 f, u8 g, u8 h) {
    return a + b + c + d + e + f + g + h;
}

/* i16,i8,i16,i8,i16 → alternating types on stack */
u16 sdcc_alt_types(u16 a, u8 b, u16 c, u8 d, u16 e) {
    return a + b + c + d + e;
}
