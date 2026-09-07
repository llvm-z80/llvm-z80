/* Test 25: structs across the z88dk conventions - Clang main calls SDCC.
 * expect 0x003F */
typedef unsigned char u8;
typedef unsigned short u16;

typedef struct { u16 x, y; } P16;
typedef struct { u8 a, b, c; } S3;
typedef struct { u16 a, b, c; } Big;

#define SC   __attribute__((smallc))
#define ZC   __attribute__((z88dk_callee))
#define C0   __attribute__((sdcccall(0)))

extern u16 sdcc_st_c0_val(P16 p, u16 t) C0;
extern u16 sdcc_st_c0_odd(S3 s, u8 t) C0;
extern u16 sdcc_st_zc_val(P16 p, u16 t) ZC;
extern u16 sdcc_st_c0c_val(P16 p, u16 t) C0 ZC;
extern u16 sdcc_st_sc_val(P16 p, u16 t) SC;
extern u16 sdcc_st_scc_val(P16 p, u16 t) SC ZC;

extern Big sdcc_st_c0_ret(u16 a, u16 b) C0;
extern Big sdcc_st_sc_ret(u16 a, u16 b) SC;
extern Big sdcc_st_zc_ret(u16 a, u16 b) ZC;
extern Big sdcc_st_c0c_ret(u16 a, u16 b) C0 ZC;
extern Big sdcc_st_scc_ret(u16 a, u16 b) SC ZC;
extern Big sdcc_st_sc_ret3(u16 a, u16 b, u16 c) SC;
extern Big sdcc_st_scc_ret3(u16 a, u16 b, u16 c) SC ZC;

static int big_is(Big b, u16 a0, u16 a1, u16 a2) {
    return b.a == a0 && b.b == a1 && b.c == a2;
}

int main(void) {
    volatile u16 status = 0;
    P16 p; S3 s;

    /* Bit 0: by-value struct on the right-to-left stack bases. */
    {
        p.x = 1; p.y = 2;
        s.a = 9; s.b = 8; s.c = 7;
        if (sdcc_st_c0_val(p, 3) == 123 && sdcc_st_c0_odd(s, 6) == 9876)
            status |= (1 << 0);
    }

    /* Bit 1: by-value struct with the callee doing the cleanup, on both the
     * register base and the all-stack one. */
    {
        p.x = 1; p.y = 2;
        if (sdcc_st_zc_val(p, 3) == 123 && sdcc_st_c0c_val(p, 3) == 123)
            status |= (1 << 1);
    }

    /* Bit 2: by-value struct under the left-to-right order.  Only this
     * direction is testable: SDCC cannot emit a __smallc call carrying a
     * struct, so clang is necessarily the caller. */
    {
        p.x = 1; p.y = 2;
        if (sdcc_st_sc_val(p, 3) == 123 && sdcc_st_scc_val(p, 3) == 123)
            status |= (1 << 2);
    }

    /* Bit 3: struct return on the right-to-left bases, where the hidden
     * pointer is simply the last thing pushed. */
    {
        Big r1 = sdcc_st_c0_ret(11, 22);
        Big r2 = sdcc_st_zc_ret(11, 22);
        Big r3 = sdcc_st_c0c_ret(11, 22);
        if (big_is(r1, 11, 22, 33) && big_is(r2, 11, 22, 33) &&
            big_is(r3, 11, 22, 33))
            status |= (1 << 3);
    }

    /* Bit 4: struct return under __smallc.  The declared arguments reverse
     * but the hidden pointer does not, so it stays right above the return
     * address with the arguments mirrored on top of it. */
    {
        Big r1 = sdcc_st_sc_ret(11, 22);
        Big r2 = sdcc_st_scc_ret(11, 22);
        if (big_is(r1, 11, 22, 33) && big_is(r2, 11, 22, 33))
            status |= (1 << 4);
    }

    /* Bit 5: the same with three declared arguments, so a hidden pointer
     * mirrored along with them would land on a real argument rather than
     * just past the end. */
    {
        Big r1 = sdcc_st_sc_ret3(1, 2, 3);
        Big r2 = sdcc_st_scc_ret3(1, 2, 3);
        if (big_is(r1, 1, 2, 3) && big_is(r2, 1, 2, 3))
            status |= (1 << 5);
    }

    return status; /* expect 0x003F */
}
