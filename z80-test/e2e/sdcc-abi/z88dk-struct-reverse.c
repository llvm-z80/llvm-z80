// RUN: rm -rf %t && split-file %s %t
// RUN: %{sdcc} -c %t/sdcc.c -o %t/sdcc.rel
// RUN: %cc %t/clang.c %t/sdcc.rel -Wl,-l,%{sdcc-lib} -o %t/prog
// RUN: %run --expect-result=0x000F %t/prog

//--- clang.c
/* SDCC main calls Clang __sdcccall(0), __smallc and __z88dk_callee
 * functions, alone and combined, with by-value struct arguments and struct
 * returns through a hidden pointer. z88dk-struct.c covers __smallc by-value
 * structs, since SDCC cannot emit that call. */
typedef unsigned char u8;
typedef unsigned short u16;

typedef struct { u16 x, y; } P16;
typedef struct { u8 a, b, c; } S3;
typedef struct { u16 a, b, c; } Big;

#define SC   __attribute__((smallc))
#define ZC   __attribute__((z88dk_callee))
#define C0   __attribute__((sdcccall(0)))

C0 u16 clang_st_c0_val(P16 p, u16 t) {
    return (u16)(p.x * 100u + p.y * 10u + t);
}

C0 u16 clang_st_c0_odd(S3 s, u8 t) {
    return (u16)(s.a * 1000u + s.b * 100u + s.c * 10u + t);
}

ZC u16 clang_st_zc_val(P16 p, u16 t) {
    return (u16)(p.x * 100u + p.y * 10u + t);
}

C0 ZC u16 clang_st_c0c_val(P16 p, u16 t) {
    return (u16)(p.x * 100u + p.y * 10u + t);
}

C0 Big clang_st_c0_ret(u16 a, u16 b) {
    Big r; r.a = a; r.b = b; r.c = (u16)(a + b); return r;
}

SC Big clang_st_sc_ret(u16 a, u16 b) {
    Big r; r.a = a; r.b = b; r.c = (u16)(a + b); return r;
}

ZC Big clang_st_zc_ret(u16 a, u16 b) {
    Big r; r.a = a; r.b = b; r.c = (u16)(a + b); return r;
}

C0 ZC Big clang_st_c0c_ret(u16 a, u16 b) {
    Big r; r.a = a; r.b = b; r.c = (u16)(a + b); return r;
}

SC ZC Big clang_st_scc_ret(u16 a, u16 b) {
    Big r; r.a = a; r.b = b; r.c = (u16)(a + b); return r;
}

/* Three declared arguments, so a hidden pointer mirrored with the arguments
 * would collide with a real argument. */
SC Big clang_st_sc_ret3(u16 a, u16 b, u16 c) {
    Big r; r.a = a; r.b = b; r.c = c; return r;
}

SC ZC Big clang_st_scc_ret3(u16 a, u16 b, u16 c) {
    Big r; r.a = a; r.b = b; r.c = c; return r;
}

//--- sdcc.c
typedef unsigned char u8;
typedef unsigned short u16;

typedef struct { u16 x, y; } P16;
typedef struct { u8 a, b, c; } S3;
typedef struct { u16 a, b, c; } Big;

extern u16 clang_st_c0_val(P16 p, u16 t) __sdcccall(0);
extern u16 clang_st_c0_odd(S3 s, u8 t) __sdcccall(0);
extern u16 clang_st_zc_val(P16 p, u16 t) __z88dk_callee;
extern u16 clang_st_c0c_val(P16 p, u16 t) __sdcccall(0) __z88dk_callee;

extern Big clang_st_c0_ret(u16 a, u16 b) __sdcccall(0);
extern Big clang_st_sc_ret(u16 a, u16 b) __smallc;
extern Big clang_st_zc_ret(u16 a, u16 b) __z88dk_callee;
extern Big clang_st_c0c_ret(u16 a, u16 b) __sdcccall(0) __z88dk_callee;
extern Big clang_st_scc_ret(u16 a, u16 b) __smallc __z88dk_callee;
extern Big clang_st_sc_ret3(u16 a, u16 b, u16 c) __smallc;
extern Big clang_st_scc_ret3(u16 a, u16 b, u16 c) __smallc __z88dk_callee;

/* Assigning a returned struct to a local makes SDCC call __memcpy, so the test
 * also links __memcpy from the runtime. */
static u8 big_is(Big b, u16 a0, u16 a1, u16 a2) {
    return (u8)(b.a == a0 && b.b == a1 && b.c == a2);
}

int main(void) {
    u16 status = 0;
    P16 p; S3 s;

    /* Bit 0: by-value struct, caller cleanup. */
    {
        p.x = 1; p.y = 2;
        s.a = 9; s.b = 8; s.c = 7;
        if (clang_st_c0_val(p, 3) == 123 && clang_st_c0_odd(s, 6) == 9876)
            status |= (1 << 0);
    }

    /* Bit 1: by-value struct with callee cleanup, under __z88dk_callee alone
     * and with __sdcccall(0). */
    {
        p.x = 1; p.y = 2;
        if (clang_st_zc_val(p, 3) == 123 && clang_st_c0c_val(p, 3) == 123)
            status |= (1 << 1);
    }

    /* Bit 2: struct return under the right-to-left conventions. */
    {
        Big r1 = clang_st_c0_ret(11, 22);
        Big r2 = clang_st_zc_ret(11, 22);
        Big r3 = clang_st_c0c_ret(11, 22);
        if (big_is(r1, 11, 22, 33) && big_is(r2, 11, 22, 33) &&
            big_is(r3, 11, 22, 33))
            status |= (1 << 2);
    }

    /* Bit 3: struct return under __smallc, with two and three declared
     * arguments and the hidden pointer pushed last. */
    {
        Big r1 = clang_st_sc_ret(11, 22);
        Big r2 = clang_st_scc_ret(11, 22);
        Big r3 = clang_st_sc_ret3(1, 2, 3);
        Big r4 = clang_st_scc_ret3(1, 2, 3);
        if (big_is(r1, 11, 22, 33) && big_is(r2, 11, 22, 33) &&
            big_is(r3, 1, 2, 3) && big_is(r4, 1, 2, 3))
            status |= (1 << 3);
    }

    return status;
}
