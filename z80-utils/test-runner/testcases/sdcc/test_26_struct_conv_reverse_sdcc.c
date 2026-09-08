/* Test 26: structs across the z88dk conventions, reverse - SDCC main calls
 * Clang.  By-value structs appear only on the right-to-left bases here:
 * SDCC cannot emit a __smallc call carrying one, so that direction is
 * covered from the clang side in test 25.
 * expect 0x000F */
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

/* Assigning the returned struct to a local makes SDCC call __memcpy, its own
 * name for the routine this runtime already provides, covering that link
 * path as well as the hidden-pointer ABI. */
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

    /* Bit 1: by-value struct, callee cleanup, on both bases. */
    {
        p.x = 1; p.y = 2;
        if (clang_st_zc_val(p, 3) == 123 && clang_st_c0c_val(p, 3) == 123)
            status |= (1 << 1);
    }

    /* Bit 2: struct return on the right-to-left bases. */
    {
        Big r1 = clang_st_c0_ret(11, 22);
        Big r2 = clang_st_zc_ret(11, 22);
        Big r3 = clang_st_c0c_ret(11, 22);
        if (big_is(r1, 11, 22, 33) && big_is(r2, 11, 22, 33) &&
            big_is(r3, 11, 22, 33))
            status |= (1 << 2);
    }

    /* Bit 3: struct return under __smallc, with two and three declared
     * arguments.  This is the case that pins where the hidden pointer sits:
     * SDCC pushes it last, below the mirrored arguments. */
    {
        Big r1 = clang_st_sc_ret(11, 22);
        Big r2 = clang_st_scc_ret(11, 22);
        Big r3 = clang_st_sc_ret3(1, 2, 3);
        Big r4 = clang_st_scc_ret3(1, 2, 3);
        if (big_is(r1, 11, 22, 33) && big_is(r2, 11, 22, 33) &&
            big_is(r3, 1, 2, 3) && big_is(r4, 1, 2, 3))
            status |= (1 << 3);
    }

    return status; /* expect 0x000F */
}
