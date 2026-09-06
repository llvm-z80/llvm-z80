/* SDCC functions for function pointer / callback tests */
typedef unsigned char u8;
typedef unsigned short u16;

/* Simple function to be passed as callback */
u16 sdcc_double(u16 x) {
    return x * 2;
}

u16 sdcc_add10(u16 x) {
    return x + 10;
}

u8 sdcc_inc(u8 x) {
    return x + 1;
}

/* Apply a function pointer to an array (SDCC calls Clang callback) */
u16 sdcc_apply_sum(u16 (*fn)(u16), u16 a, u16 b, u16 c) {
    return fn(a) + fn(b) + fn(c);
}

/* Higher-order: SDCC takes i8 callback, applies to values */
u8 sdcc_map_sum_i8(u8 (*fn)(u8), u8 a, u8 b, u8 c, u8 d) {
    return fn(a) + fn(b) + fn(c) + fn(d);
}

/* Dispatch: call one of two callbacks based on selector */
u16 sdcc_dispatch(u16 (*fn_a)(u16), u16 (*fn_b)(u16), u8 sel, u16 val) {
    if (sel)
        return fn_a(val);
    else
        return fn_b(val);
}
