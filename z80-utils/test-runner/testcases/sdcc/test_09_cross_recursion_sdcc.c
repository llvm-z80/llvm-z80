/* SDCC functions for cross-boundary recursion tests */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

extern u16 clang_mutual_rec(u16 n);

/* Mutual recursion: SDCC → Clang → SDCC → ... */
u16 sdcc_mutual_rec(u16 n) {
    if (n == 0)
        return 0;
    return n + clang_mutual_rec(n - 1);
}

extern u32 clang_mutual_rec32(u32 n);

/* i32 mutual recursion */
u32 sdcc_mutual_rec32(u32 n) {
    if (n == 0)
        return 0;
    return n + clang_mutual_rec32(n - 1);
}

/* Deep cross-call: SDCC accumulates then calls Clang */
extern u16 clang_deep_add(u16 a, u16 b, u16 c);

u16 sdcc_deep_chain(u16 a, u16 b, u16 c, u16 d) {
    u16 tmp = clang_deep_add(a, b, c);
    return tmp + d;
}
