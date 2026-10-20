// RUN: rm -rf %t && split-file %s %t
// RUN: %{sdcc} -c %t/sdcc.c -o %t/sdcc.rel
// RUN: %cc %t/clang.c %t/sdcc.rel -Wl,-l,%{sdcc-lib} -o %t/prog
// RUN: %run --expect-result=0x000F %t/prog

//--- clang.c
/* Clang and SDCC functions call each other recursively with i16 and i32
 * values, and Clang main calls an SDCC function that calls back into Clang. */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

extern u16 sdcc_mutual_rec(u16 n);
extern u32 sdcc_mutual_rec32(u32 n);
extern u16 sdcc_deep_chain(u16 a, u16 b, u16 c, u16 d);

u16 clang_mutual_rec(u16 n) {
    if (n == 0)
        return 0;
    return n + sdcc_mutual_rec(n - 1);
}

u32 clang_mutual_rec32(u32 n) {
    if (n == 0)
        return 0;
    return n + sdcc_mutual_rec32(n - 1);
}

u16 clang_deep_add(u16 a, u16 b, u16 c) {
    return a + b + c;
}

int main(void) {
    volatile u16 status = 0;

    /* Bit 0: i16 mutual recursion (sum 1..5 = 15) */
    {
        volatile u16 r = sdcc_mutual_rec(5);
        if (r == 15)
            status |= (1 << 0);
    }

    /* Bit 1: i16 mutual recursion from Clang entry */
    {
        volatile u16 r = clang_mutual_rec(6);
        if (r == 21)
            status |= (1 << 1);
    }

    /* Bit 2: i32 mutual recursion (sum 1..4 = 10) */
    {
        volatile u32 r = sdcc_mutual_rec32(4);
        if (r == 10)
            status |= (1 << 2);
    }

    /* Bit 3: chain from SDCC into Clang and back, with stack args */
    {
        volatile u16 r1 = sdcc_deep_chain(10, 20, 30, 40);
        volatile u16 r2 = sdcc_deep_chain(100, 200, 300, 400);
        if (r1 == 100 && r2 == 1000)
            status |= (1 << 3);
    }

    return status;
}

//--- sdcc.c
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

extern u16 clang_mutual_rec(u16 n);

u16 sdcc_mutual_rec(u16 n) {
    if (n == 0)
        return 0;
    return n + clang_mutual_rec(n - 1);
}

extern u32 clang_mutual_rec32(u32 n);

u32 sdcc_mutual_rec32(u32 n) {
    if (n == 0)
        return 0;
    return n + clang_mutual_rec32(n - 1);
}

extern u16 clang_deep_add(u16 a, u16 b, u16 c);

u16 sdcc_deep_chain(u16 a, u16 b, u16 c, u16 d) {
    u16 tmp = clang_deep_add(a, b, c);
    return tmp + d;
}
