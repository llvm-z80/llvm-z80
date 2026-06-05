/* Test 13: Struct via pointer and array operations across SDCC/Clang boundary */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

typedef struct { u8 x; u8 y; } Point8;
typedef struct { u16 x; u16 y; } Point16;
typedef struct { u32 lo; u32 hi; } Pair32;

extern u16 sdcc_point8_sum(Point8 *p);
extern void sdcc_point8_set(Point8 *p, u8 x, u8 y);
extern u16 sdcc_point16_sum(Point16 *p);
extern void sdcc_point16_swap(Point16 *p);
extern u32 sdcc_pair32_sum(Pair32 *p);
extern void sdcc_pair32_swap(Pair32 *p);
extern u16 sdcc_array_u8_sum(u8 *arr, u8 len);
extern void sdcc_array_u16_fill(u16 *arr, u16 val, u8 count);
extern u16 sdcc_array_dot(u8 *a, u8 *b, u8 len);

int main(void) {
    volatile u16 status = 0;

    /* Bit 0: Point8 read/write via pointer */
    {
        volatile Point8 p;
        sdcc_point8_set((Point8 *)&p, 10, 20);
        volatile u16 s = sdcc_point8_sum((Point8 *)&p);
        if (p.x == 10 && p.y == 20 && s == 30)
            status |= (1 << 0);
    }

    /* Bit 1: Point16 sum and swap via pointer */
    {
        volatile Point16 p = {0x1234, 0x5678};
        volatile u16 s = sdcc_point16_sum((Point16 *)&p);
        sdcc_point16_swap((Point16 *)&p);
        if (s == 0x68AC && p.x == 0x5678 && p.y == 0x1234)
            status |= (1 << 1);
    }

    /* Bit 2: Pair32 sum and swap via pointer */
    {
        volatile Pair32 p = {0x00010000UL, 0x00020000UL};
        volatile u32 s = sdcc_pair32_sum((Pair32 *)&p);
        sdcc_pair32_swap((Pair32 *)&p);
        if (s == 0x00030000UL && p.lo == 0x00020000UL && p.hi == 0x00010000UL)
            status |= (1 << 2);
    }

    /* Bit 3: Array operations */
    {
        volatile u8 arr[] = {1, 2, 3, 4, 5};
        volatile u16 s = sdcc_array_u8_sum((u8 *)arr, 5);

        volatile u16 buf[3];
        sdcc_array_u16_fill((u16 *)buf, 0xABCD, 3);

        volatile u8 a[] = {1, 2, 3};
        volatile u8 b[] = {4, 5, 6};
        volatile u16 dot = sdcc_array_dot((u8 *)a, (u8 *)b, 3);

        if (s == 15 && buf[0] == 0xABCD && buf[1] == 0xABCD &&
            buf[2] == 0xABCD && dot == 32)
            status |= (1 << 3);
    }

    return status; /* expect 0x000F */
}
