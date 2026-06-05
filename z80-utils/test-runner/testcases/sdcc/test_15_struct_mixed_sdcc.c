/* SDCC functions for struct by-value + scalar mixing tests */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

typedef struct { u8 x; u8 y; } Point8;
typedef struct { u16 x; u16 y; } Point16;
typedef struct { u8 a; u8 b; u8 c; u8 d; u8 e; } Big5;  /* 5 bytes (odd) */
typedef struct { u8 a; u16 b; u16 c; u8 d; } Mixed8;     /* 6 bytes */

/* Struct first, scalar second: scalar must go to stack (position consumed) */
u16 sdcc_struct_then_u8(Point8 p, u8 x) {
    return (u16)p.x + (u16)p.y + (u16)x;
}

u16 sdcc_struct_then_u16(Point8 p, u16 x) {
    return (u16)p.x + (u16)p.y + x;
}

/* Scalar first, struct second: scalar in register, struct on stack */
u16 sdcc_u8_then_struct(u8 x, Point8 p) {
    return (u16)x + (u16)p.x + (u16)p.y;
}

u16 sdcc_u16_then_struct(u16 x, Point8 p) {
    return x + (u16)p.x + (u16)p.y;
}

/* Large struct (5 bytes, odd size) */
u16 sdcc_big5_sum(Big5 b) {
    return (u16)b.a + (u16)b.b + (u16)b.c + (u16)b.d + (u16)b.e;
}

/* Mixed struct (6 bytes, has u16 fields) */
u16 sdcc_mixed8_sum(Mixed8 m) {
    return (u16)m.a + m.b + m.c + (u16)m.d;
}

/* Struct + struct: both on stack */
u16 sdcc_two_structs(Point8 p1, Point8 p2) {
    return (u16)p1.x + (u16)p1.y + (u16)p2.x + (u16)p2.y;
}

/* Struct return + byval arg combined */
Point8 sdcc_struct_mirror(Point8 p) {
    Point8 r;
    r.x = p.y;
    r.y = p.x;
    return r;
}
