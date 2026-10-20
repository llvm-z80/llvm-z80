// RUN: rm -rf %t && split-file %s %t
// RUN: %{sdcc} -c %t/sdcc.c -o %t/sdcc.rel
// RUN: %cc %t/clang.c %t/sdcc.rel -Wl,-l,%{sdcc-lib} -o %t/prog
// RUN: %run --expect-result=0x000F %t/prog

//--- clang.c
/* Clang main calls SDCC functions with structs passed and returned by
 * value. */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

typedef struct { u8 x; u8 y; } Point8;
typedef struct { u16 x; u16 y; } Point16;

extern u16 sdcc_point8_val_sum(Point8 p);
extern u32 sdcc_point16_val_sum(Point16 p);
extern Point8 sdcc_make_point8(u8 x, u8 y);
extern Point16 sdcc_make_point16(u16 x, u16 y);
extern Point8 sdcc_point8_mirror(Point8 p);

int main(void) {
    volatile u16 status = 0;

    /* Bit 0: Point8 by-value sum */
    {
        Point8 p = {10, 20};
        volatile u16 r = sdcc_point8_val_sum(p);
        if (r == 30)
            status |= (1 << 0);
    }

    /* Bit 1: Point16 by-value sum */
    {
        Point16 p = {0x1000, 0x2000};
        volatile u32 r = sdcc_point16_val_sum(p);
        if (r == 0x3000UL)
            status |= (1 << 1);
    }

    /* Bit 2: struct return (Point8) */
    {
        volatile Point8 p = sdcc_make_point8(42, 99);
        if (p.x == 42 && p.y == 99)
            status |= (1 << 2);
    }

    /* Bit 3: struct return (Point16) */
    {
        volatile Point16 p = sdcc_make_point16(0xAAAA, 0x5555);
        if (p.x == 0xAAAA && p.y == 0x5555)
            status |= (1 << 3);
    }

    return status;
}

//--- sdcc.c
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

typedef struct { u8 x; u8 y; } Point8;
typedef struct { u16 x; u16 y; } Point16;

/* Struct by value: struct bytes on the stack */
u16 sdcc_point8_val_sum(Point8 p) {
    return (u16)p.x + (u16)p.y;
}

u32 sdcc_point16_val_sum(Point16 p) {
    return (u32)p.x + (u32)p.y;
}

/* Struct return: sret pointer on the stack */
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

Point8 sdcc_point8_mirror(Point8 p) {
    Point8 r;
    r.x = p.y;
    r.y = p.x;
    return r;
}
