/* Test 15: Struct by-value + scalar mixing, large/odd structs, sret+byval */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

typedef struct { u8 x; u8 y; } Point8;
typedef struct { u16 x; u16 y; } Point16;
typedef struct { u8 a; u8 b; u8 c; u8 d; u8 e; } Big5;
typedef struct { u8 a; u16 b; u16 c; u8 d; } Mixed8;

extern u16 sdcc_struct_then_u8(Point8 p, u8 x);
extern u16 sdcc_struct_then_u16(Point8 p, u16 x);
extern u16 sdcc_u8_then_struct(u8 x, Point8 p);
extern u16 sdcc_u16_then_struct(u16 x, Point8 p);
extern u16 sdcc_big5_sum(Big5 b);
extern u16 sdcc_mixed8_sum(Mixed8 m);
extern u16 sdcc_two_structs(Point8 p1, Point8 p2);
extern Point8 sdcc_struct_mirror(Point8 p);

int main(void) {
    volatile u16 status = 0;

    /* Bit 0: struct first + u8 second (both on stack, position consumed) */
    {
        Point8 p = {10, 20};
        volatile u16 r = sdcc_struct_then_u8(p, 5);
        if (r == 35)
            status |= (1 << 0);
    }

    /* Bit 1: struct first + u16 second (both on stack) */
    {
        Point8 p = {3, 7};
        volatile u16 r = sdcc_struct_then_u16(p, 0x1000);
        if (r == 0x100A)
            status |= (1 << 1);
    }

    /* Bit 2: u8 first (A) + struct second (stack) */
    {
        Point8 p = {10, 20};
        volatile u16 r = sdcc_u8_then_struct(5, p);
        if (r == 35)
            status |= (1 << 2);
    }

    /* Bit 3: u16 first (HL/DE) + struct second (stack) */
    {
        Point8 p = {3, 7};
        volatile u16 r = sdcc_u16_then_struct(0x1000, p);
        if (r == 0x100A)
            status |= (1 << 3);
    }

    /* Bit 4: large odd-sized struct (5 bytes) */
    {
        Big5 b = {1, 2, 3, 4, 5};
        volatile u16 r = sdcc_big5_sum(b);
        if (r == 15)
            status |= (1 << 4);
    }

    /* Bit 5: mixed struct with u16 fields (6 bytes) */
    {
        Mixed8 m = {1, 0x0200, 0x0300, 4};
        volatile u16 r = sdcc_mixed8_sum(m);
        if (r == 0x0505)
            status |= (1 << 5);
    }

    /* Bit 6: two structs (both on stack) */
    {
        Point8 p1 = {10, 20};
        Point8 p2 = {30, 40};
        volatile u16 r = sdcc_two_structs(p1, p2);
        if (r == 100)
            status |= (1 << 6);
    }

    /* Bit 7: sret + byval combined (struct return with struct arg) */
    {
        Point8 p = {42, 99};
        volatile Point8 r = sdcc_struct_mirror(p);
        if (r.x == 99 && r.y == 42)
            status |= (1 << 7);
    }

    return status; /* expect 0x00FF */
}
