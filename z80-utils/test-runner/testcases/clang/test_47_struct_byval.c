/* Test 47: Struct by-value passing (Clang-to-Clang byval) */
typedef unsigned char u8;
typedef unsigned short u16;

typedef struct { u8 x; u8 y; } Point8;
typedef struct { u16 x; u16 y; } Point16;
typedef struct { u8 a; u8 b; u8 c; u8 d; u8 e; } Big5;

/* --- Callees --- */

__attribute__((noinline))
u16 point8_sum(Point8 p) {
    return (u16)p.x + (u16)p.y;
}

__attribute__((noinline))
u16 struct_then_u8(Point8 p, u8 x) {
    return (u16)p.x + (u16)p.y + (u16)x;
}

__attribute__((noinline))
u16 u8_then_struct(u8 x, Point8 p) {
    return (u16)x + (u16)p.x + (u16)p.y;
}

__attribute__((noinline))
u16 point16_sum(Point16 p) {
    return p.x + p.y;
}

__attribute__((noinline))
u16 big5_sum(Big5 b) {
    return (u16)b.a + (u16)b.b + (u16)b.c + (u16)b.d + (u16)b.e;
}

__attribute__((noinline))
Point8 mirror(Point8 p) {
    Point8 r;
    r.x = p.y;
    r.y = p.x;
    return r;
}

__attribute__((noinline))
u16 two_structs(Point8 p1, Point8 p2) {
    return (u16)p1.x + (u16)p1.y + (u16)p2.x + (u16)p2.y;
}

/* --- Main --- */

int main(void) {
    u16 status = 0;

    /* Bit 0: basic byval */
    {
        Point8 p = {10, 20};
        volatile u16 r = point8_sum(p);
        if (r == 30)
            status |= (1 << 0);
    }

    /* Bit 1: struct first + scalar (position consumed, both on stack) */
    {
        Point8 p = {3, 7};
        volatile u16 r = struct_then_u8(p, 5);
        if (r == 15)
            status |= (1 << 1);
    }

    /* Bit 2: scalar first (register) + struct (stack) */
    {
        Point8 p = {10, 20};
        volatile u16 r = u8_then_struct(5, p);
        if (r == 35)
            status |= (1 << 2);
    }

    /* Bit 3: 4-byte struct (Point16) */
    {
        Point16 p = {0x1000, 0x2000};
        volatile u16 r = point16_sum(p);
        if (r == 0x3000)
            status |= (1 << 3);
    }

    /* Bit 4: 5-byte odd-sized struct */
    {
        Big5 b = {1, 2, 3, 4, 5};
        volatile u16 r = big5_sum(b);
        if (r == 15)
            status |= (1 << 4);
    }

    /* Bit 5: sret + byval combined (struct return with struct arg) */
    {
        Point8 p = {42, 99};
        volatile Point8 r = mirror(p);
        if (r.x == 99 && r.y == 42)
            status |= (1 << 5);
    }

    /* Bit 6: two structs */
    {
        Point8 p1 = {10, 20};
        Point8 p2 = {30, 40};
        volatile u16 r = two_structs(p1, p2);
        if (r == 100)
            status |= (1 << 6);
    }

    return status; /* expect 0x007F */
}
