/* SDCC functions for struct/array tests via pointer */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

typedef struct { u8 x; u8 y; } Point8;
typedef struct { u16 x; u16 y; } Point16;
typedef struct { u32 lo; u32 hi; } Pair32;

/* --- Struct via pointer --- */

/* Read struct fields via pointer */
u16 sdcc_point8_sum(Point8 *p) {
    return (u16)p->x + (u16)p->y;
}

/* Write struct fields via pointer */
void sdcc_point8_set(Point8 *p, u8 x, u8 y) {
    p->x = x;
    p->y = y;
}

/* Read 16-bit struct fields */
u16 sdcc_point16_sum(Point16 *p) {
    return p->x + p->y;
}

/* Swap struct fields in place */
void sdcc_point16_swap(Point16 *p) {
    u16 tmp = p->x;
    p->x = p->y;
    p->y = tmp;
}

/* 32-bit struct operations */
u32 sdcc_pair32_sum(Pair32 *p) {
    return p->lo + p->hi;
}

void sdcc_pair32_swap(Pair32 *p) {
    u32 tmp = p->lo;
    p->lo = p->hi;
    p->hi = tmp;
}

/* --- Array operations --- */

/* Sum u8 array */
u16 sdcc_array_u8_sum(u8 *arr, u8 len) {
    u16 sum = 0;
    u8 i;
    for (i = 0; i < len; i++)
        sum += arr[i];
    return sum;
}

/* Fill u16 array with value */
void sdcc_array_u16_fill(u16 *arr, u16 val, u8 count) {
    u8 i;
    for (i = 0; i < count; i++)
        arr[i] = val;
}

/* Dot product of two u8 arrays */
u16 sdcc_array_dot(u8 *a, u8 *b, u8 len) {
    u16 sum = 0;
    u8 i;
    for (i = 0; i < len; i++)
        sum += (u16)a[i] * (u16)b[i];
    return sum;
}
