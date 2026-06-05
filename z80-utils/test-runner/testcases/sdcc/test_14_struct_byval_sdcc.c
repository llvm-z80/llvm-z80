/* SDCC functions for struct by-value and struct return tests */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

typedef struct { u8 x; u8 y; } Point8;
typedef struct { u16 x; u16 y; } Point16;

/* Struct by value (SDCC pushes struct bytes on stack) */
u16 sdcc_point8_val_sum(Point8 p) {
    return (u16)p.x + (u16)p.y;
}

u32 sdcc_point16_val_sum(Point16 p) {
    return (u32)p.x + (u32)p.y;
}

/* Struct return (SDCC uses sret on stack) */
Point8 sdcc_make_point8(u8 x, u8 y) {
    Point8 p;
    p.x = x;
    p.y = y;
    return p;
}

Point16 sdcc_make_point16(u16 x, u16 y) {
    Point16 p;
    p.x = x;
    p.y = y;
    return p;
}

/* Struct pass-through: take by value, return by value */
Point8 sdcc_point8_mirror(Point8 p) {
    Point8 r;
    r.x = p.y;
    r.y = p.x;
    return r;
}
