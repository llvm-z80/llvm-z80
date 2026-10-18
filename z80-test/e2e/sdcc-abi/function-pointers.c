// RUN: rm -rf %t && split-file %s %t
// RUN: %{sdcc} -c %t/sdcc.c -o %t/sdcc.rel
// RUN: %cc %t/clang.c %t/sdcc.rel -Wl,-l,%{sdcc-lib} -o %t/prog
// RUN: %run --expect-result=0x000F %t/prog

//--- clang.c
/* Clang main passes Clang and SDCC function pointers to SDCC and Clang
 * higher-order functions, so callbacks cross the boundary both ways. */
typedef unsigned char u8;
typedef unsigned short u16;

/* SDCC callbacks */
extern u16 sdcc_double(u16 x);
extern u16 sdcc_add10(u16 x);
extern u8 sdcc_inc(u8 x);

/* SDCC higher-order functions */
extern u16 sdcc_apply_sum(u16 (*fn)(u16), u16 a, u16 b, u16 c);
extern u8 sdcc_map_sum_i8(u8 (*fn)(u8), u8 a, u8 b, u8 c, u8 d);
extern u16 sdcc_dispatch(u16 (*fn_a)(u16), u16 (*fn_b)(u16), u8 sel, u16 val);

/* Clang callbacks */
u16 clang_triple(u16 x) {
    return x * 3;
}

u8 clang_dec(u8 x) {
    return x - 1;
}

/* Clang higher-order function */
u16 clang_apply_add(u16 (*fn)(u16), u16 base, u16 x) {
    return base + fn(x);
}

int main(void) {
    volatile u16 status = 0;

    /* Bit 0: Clang calls SDCC callbacks directly */
    {
        volatile u16 r1 = sdcc_double(50);
        volatile u16 r2 = sdcc_add10(90);
        if (r1 == 100 && r2 == 100)
            status |= (1 << 0);
    }

    /* Bit 1: SDCC higher-order function with a Clang callback */
    {
        /* 30+60+90 = 180 */
        volatile u16 r = sdcc_apply_sum(&clang_triple, 10, 20, 30);
        if (r == 180)
            status |= (1 << 1);
    }

    /* Bit 2: SDCC higher-order functions with SDCC and i8 Clang callbacks */
    {
        /* 10+20+30 = 60 */
        volatile u16 r1 = sdcc_apply_sum(&sdcc_double, 5, 10, 15);
        /* 9+19+29+39 = 96 */
        volatile u8 r2 = sdcc_map_sum_i8(&clang_dec, 10, 20, 30, 40);
        if (r1 == 60 && r2 == 96)
            status |= (1 << 2);
    }

    /* Bit 3: dispatch with mixed SDCC and Clang callbacks */
    {
        volatile u16 r1 = sdcc_dispatch(&sdcc_double, &clang_triple, 1, 25);
        volatile u16 r2 = sdcc_dispatch(&sdcc_double, &clang_triple, 0, 25);
        volatile u16 r3 = clang_apply_add(&sdcc_add10, 1000, 50);
        if (r1 == 50 && r2 == 75 && r3 == 1060)
            status |= (1 << 3);
    }

    return status;
}

//--- sdcc.c
typedef unsigned char u8;
typedef unsigned short u16;

/* SDCC callbacks */
u16 sdcc_double(u16 x) {
    return x * 2;
}

u16 sdcc_add10(u16 x) {
    return x + 10;
}

u8 sdcc_inc(u8 x) {
    return x + 1;
}

u16 sdcc_apply_sum(u16 (*fn)(u16), u16 a, u16 b, u16 c) {
    return fn(a) + fn(b) + fn(c);
}

u8 sdcc_map_sum_i8(u8 (*fn)(u8), u8 a, u8 b, u8 c, u8 d) {
    return fn(a) + fn(b) + fn(c) + fn(d);
}

u16 sdcc_dispatch(u16 (*fn_a)(u16), u16 (*fn_b)(u16), u8 sel, u16 val) {
    if (sel)
        return fn_a(val);
    else
        return fn_b(val);
}
