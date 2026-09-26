/* Test 67: Ordered comparisons of 8, 16 and 32-bit values, against constants
 * and against each other.
 *
 * Before selection, on a byte x > c becomes x >= c + 1 and x <= c becomes
 * x < c + 1, a signed order becomes the unsigned one with the sign bit of both
 * sides flipped, and an order against 0 or -1 becomes a test of the sign bit
 * alone.  Each predicate is checked in a branch and as a value, for every byte
 * against constants at and around the edges where those rewrites change
 * shape, and for wider values around the same edges.
 *
 * The references compare with the bias taken from a volatile, which the
 * compiler cannot see through to fold into the order being tested. */
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef __INT32_TYPE__ int32_t;
typedef __UINT32_TYPE__ uint32_t;

static volatile uint8_t bias8 = 0x80;
static volatile uint16_t bias16 = 0x8000;
static volatile uint8_t zero8 = 0;
static volatile uint16_t zero16 = 0;
static volatile uint32_t bias32 = 0x80000000u;
static volatile uint32_t zero32 = 0;

__attribute__((noinline)) static uint8_t ref_lt8(uint8_t a, uint8_t b,
                                                 uint8_t flip) {
    return (uint8_t)(a ^ flip) < (uint8_t)(b ^ flip);
}

__attribute__((noinline)) static uint8_t ref_lt16(uint16_t a, uint16_t b,
                                                  uint16_t flip) {
    return (uint16_t)(a ^ flip) < (uint16_t)(b ^ flip);
}

__attribute__((noinline)) static uint8_t ref_lt32(uint32_t a, uint32_t b,
                                                  uint32_t flip) {
    return (uint32_t)(a ^ flip) < (uint32_t)(b ^ flip);
}

static uint8_t marks;

__attribute__((noinline)) static void mark(uint8_t bit) { marks |= bit; }

/* Branch form: a call on each side keeps the compare feeding a branch. */
#define BRANCH(T, N, C)                                                       \
    __attribute__((noinline)) static uint8_t br_##N(T x) {                  \
        marks = 0;                                                            \
        if (x < C) mark(1);                                                   \
        if (x <= C) mark(2);                                                  \
        if (x > C) mark(4);                                                   \
        if (x >= C) mark(8);                                                  \
        if (C < x) mark(16);                                                  \
        if (C >= x) mark(32);                                                 \
        return marks;                                                         \
    }

/* Value form: each compare on its own returns its result as a byte, which
 * keeps it from being folded into a select with the others. */
#define ONE(T, N, P, E)                                                       \
    __attribute__((noinline)) static uint8_t P##_##N(T x) { return E; }
#define VALUE(T, N, C)                                                        \
    ONE(T, N, lt, x < C)                                                      \
    ONE(T, N, le, x <= C)                                                     \
    ONE(T, N, gt, x > C)                                                      \
    ONE(T, N, ge, x >= C)                                                     \
    ONE(T, N, clt, C < x)                                                     \
    ONE(T, N, cge, C >= x)                                                    \
    static uint8_t val_##N(T x) {                                             \
        return (uint8_t)(lt_##N(x) | le_##N(x) << 1 | gt_##N(x) << 2 |       \
                         ge_##N(x) << 3 | clt_##N(x) << 4 | cge_##N(x) << 5); \
    }

#define BOTH(T, N, C) BRANCH(T, N, C) VALUE(T, N, C)

/* Signed bytes. */
BOTH(int8_t, sm128, -128)
BOTH(int8_t, sm127, -127)
BOTH(int8_t, sm2, -2)
BOTH(int8_t, sm1, -1)
BOTH(int8_t, sz, 0)
BOTH(int8_t, sp1, 1)
BOTH(int8_t, sp2, 2)
BOTH(int8_t, sp100, 100)
BOTH(int8_t, sp126, 126)
BOTH(int8_t, sp127, 127)

/* Unsigned bytes. */
BOTH(uint8_t, u0, 0)
BOTH(uint8_t, u1, 1)
BOTH(uint8_t, u127, 127)
BOTH(uint8_t, u128, 128)
BOTH(uint8_t, u254, 254)
BOTH(uint8_t, u255, 255)

/* Signed pairs. */
BOTH(int16_t, wm32768, (-32767 - 1))
BOTH(int16_t, wm257, -257)
BOTH(int16_t, wm1, -1)
BOTH(int16_t, wz, 0)
BOTH(int16_t, wp5, 5)
BOTH(int16_t, wp255, 255)
BOTH(int16_t, wp256, 256)
BOTH(int16_t, wp32767, 32767)

/* Unsigned pairs. */
BOTH(uint16_t, v0, 0)
BOTH(uint16_t, v255, 255)
BOTH(uint16_t, v32768, 32768u)
BOTH(uint16_t, v65535, 65535u)

/* 32-bit values. */
BOTH(int32_t, lmin, (-2147483647 - 1))
BOTH(int32_t, lm1, -1)
BOTH(int32_t, lz, 0)
BOTH(int32_t, lp5, 5)
BOTH(int32_t, lp65536, 65536)
BOTH(int32_t, lmax, 2147483647)
BOTH(uint32_t, lu0, 0u)
BOTH(uint32_t, lu65535, 65535u)
BOTH(uint32_t, lumax, 4294967295u)

static uint8_t expected(uint8_t lt, uint8_t gt) {
    return (uint8_t)(lt | (!gt) << 1 | gt << 2 | (!lt) << 3 | gt << 4 |
                     (!gt) << 5);
}

static uint8_t want8(uint8_t x, uint8_t c, uint8_t flip) {
    return expected(ref_lt8(x, c, flip), ref_lt8(c, x, flip));
}

static uint8_t want16(uint16_t x, uint16_t c, uint16_t flip) {
    return expected(ref_lt16(x, c, flip), ref_lt16(c, x, flip));
}

static uint8_t want32(uint32_t x, uint32_t c, uint32_t flip) {
    return expected(ref_lt32(x, c, flip), ref_lt32(c, x, flip));
}

/* Signed pairs against each other. */
__attribute__((noinline)) static uint8_t br_pair(int16_t x, int16_t y) {
    marks = 0;
    if (x < y) mark(1);
    if (x <= y) mark(2);
    if (x > y) mark(4);
    if (x >= y) mark(8);
    return marks;
}

__attribute__((noinline)) static uint8_t lt_pair(int16_t x, int16_t y) {
    return x < y;
}
__attribute__((noinline)) static uint8_t le_pair(int16_t x, int16_t y) {
    return x <= y;
}

static uint8_t val_pair(int16_t x, int16_t y) {
    return (uint8_t)(lt_pair(x, y) | le_pair(x, y) << 1 | lt_pair(y, x) << 2 |
                     le_pair(y, x) << 3);
}

/* Signed 32-bit values against each other. */
__attribute__((noinline)) static uint8_t br_pair32(int32_t x, int32_t y) {
    marks = 0;
    if (x < y) mark(1);
    if (x <= y) mark(2);
    if (x > y) mark(4);
    if (x >= y) mark(8);
    return marks;
}

__attribute__((noinline)) static uint8_t lt_pair32(int32_t x, int32_t y) {
    return x < y;
}
__attribute__((noinline)) static uint8_t le_pair32(int32_t x, int32_t y) {
    return x <= y;
}

static uint8_t val_pair32(int32_t x, int32_t y) {
    return (uint8_t)(lt_pair32(x, y) | le_pair32(x, y) << 1 |
                     lt_pair32(y, x) << 2 | le_pair32(y, x) << 3);
}

/* The sign of an address. */
__attribute__((noinline)) static uint8_t high_address(const char *p) {
    return (int16_t)(uint16_t)p < 0;
}

static const int16_t wide[] = {-32768, -32767, -258, -257, -256, -129, -128,
                               -2,     -1,     0,    1,    4,    5,    6,
                               127,    128,    254,  255,  256,  257,  32766,
                               32767};
#define NWIDE (sizeof(wide) / sizeof(wide[0]))

static const int32_t wide32[] = {(-2147483647 - 1), -2147483647, -65537,
                                 -65536, -65535, -257, -256, -2, -1, 0, 1, 5,
                                 6, 255, 256, 65535, 65536, 2147483646,
                                 2147483647};
#define NWIDE32 (sizeof(wide32) / sizeof(wide32[0]))

int main(void) {
    uint16_t status = 0;
    uint16_t bad_br = 0, bad_val = 0;
    volatile int16_t i;
    uint8_t j, k;

    for (i = -128; i <= 127; i++) {
        uint8_t x = (uint8_t)i;
#define CHECK(N, C, F)                                                        \
    if (br_##N(x) != want8(x, (uint8_t)(C), F)) bad_br++;                    \
    if (val_##N(x) != want8(x, (uint8_t)(C), F)) bad_val++;
        CHECK(sm128, -128, bias8)
        CHECK(sm127, -127, bias8)
        CHECK(sm2, -2, bias8)
        CHECK(sm1, -1, bias8)
        CHECK(sz, 0, bias8)
        CHECK(sp1, 1, bias8)
        CHECK(sp2, 2, bias8)
        CHECK(sp100, 100, bias8)
        CHECK(sp126, 126, bias8)
        CHECK(sp127, 127, bias8)
        CHECK(u0, 0, zero8)
        CHECK(u1, 1, zero8)
        CHECK(u127, 127, zero8)
        CHECK(u128, 128, zero8)
        CHECK(u254, 254, zero8)
        CHECK(u255, 255, zero8)
#undef CHECK
    }
    if (bad_br == 0) status |= 1;
    if (bad_val == 0) status |= 2;

    bad_br = bad_val = 0;
    for (j = 0; j < NWIDE; j++) {
        uint16_t x = (uint16_t)wide[j];
#define CHECK(N, C, F)                                                        \
    if (br_##N(x) != want16(x, (uint16_t)(C), F)) bad_br++;                  \
    if (val_##N(x) != want16(x, (uint16_t)(C), F)) bad_val++;
        CHECK(wm32768, (-32767 - 1), bias16)
        CHECK(wm257, -257, bias16)
        CHECK(wm1, -1, bias16)
        CHECK(wz, 0, bias16)
        CHECK(wp5, 5, bias16)
        CHECK(wp255, 255, bias16)
        CHECK(wp256, 256, bias16)
        CHECK(wp32767, 32767, bias16)
        CHECK(v0, 0, zero16)
        CHECK(v255, 255, zero16)
        CHECK(v32768, 32768u, zero16)
        CHECK(v65535, 65535u, zero16)
#undef CHECK
    }
    if (bad_br == 0) status |= 4;
    if (bad_val == 0) status |= 8;

    bad_br = bad_val = 0;
    for (j = 0; j < NWIDE; j++)
        for (k = 0; k < NWIDE; k++) {
            uint16_t x = (uint16_t)wide[j], y = (uint16_t)wide[k];
            uint8_t lt = ref_lt16(x, y, bias16), gt = ref_lt16(y, x, bias16);
            uint8_t want = (uint8_t)(lt | (!gt) << 1 | gt << 2 | (!lt) << 3);
            if (br_pair(wide[j], wide[k]) != want) bad_br++;
            if (val_pair(wide[j], wide[k]) != want) bad_val++;
        }
    if (bad_br == 0) status |= 16;
    if (bad_val == 0) status |= 32;

    bad_br = bad_val = 0;
    for (j = 0; j < NWIDE32; j++) {
        uint32_t x = (uint32_t)wide32[j];
#define CHECK(N, C, F)                                                        \
    if (br_##N(x) != want32(x, (uint32_t)(C), F)) bad_br++;                  \
    if (val_##N(x) != want32(x, (uint32_t)(C), F)) bad_val++;
        CHECK(lmin, (-2147483647 - 1), bias32)
        CHECK(lm1, -1, bias32)
        CHECK(lz, 0, bias32)
        CHECK(lp5, 5, bias32)
        CHECK(lp65536, 65536, bias32)
        CHECK(lmax, 2147483647, bias32)
        CHECK(lu0, 0u, zero32)
        CHECK(lu65535, 65535u, zero32)
        CHECK(lumax, 4294967295u, zero32)
#undef CHECK
    }
    if (bad_br == 0) status |= 128;
    if (bad_val == 0) status |= 256;

    bad_br = bad_val = 0;
    for (j = 0; j < NWIDE32; j++)
        for (k = 0; k < NWIDE32; k++) {
            uint32_t x = (uint32_t)wide32[j], y = (uint32_t)wide32[k];
            uint8_t lt = ref_lt32(x, y, bias32), gt = ref_lt32(y, x, bias32);
            uint8_t want = (uint8_t)(lt | (!gt) << 1 | gt << 2 | (!lt) << 3);
            if (br_pair32(wide32[j], wide32[k]) != want) bad_br++;
            if (val_pair32(wide32[j], wide32[k]) != want) bad_val++;
        }
    if (bad_br == 0) status |= 512;
    if (bad_val == 0) status |= 1024;

    if (!high_address((const char *)0x7FFF) &&
        high_address((const char *)0x8000) &&
        high_address((const char *)0xFFFF) && !high_address((const char *)0))
        status |= 64;

    return status; /* expect 0x07FF */
}
