/* Test 68: Unsigned 16-bit comparisons against constants.
 *
 * A constant is compared as an immediate: SUB on the low byte and SBC on
 * the high one, or CP on the high byte alone when the constant's low byte is
 * zero.  x > c is compared as x >= c + 1, which keeps the constant on the
 * right.  Every predicate is checked in branch and value form against
 * constants on both sides of a page boundary, for values one step and one
 * page around each.  The reference compares the values widened to 32 bits,
 * which takes a different path. */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef __UINT32_TYPE__ uint32_t;

static uint8_t marks;

__attribute__((noinline)) static void mark(uint8_t bit) { marks |= bit; }

#define BRANCH(N, C)                                                          \
    __attribute__((noinline)) static uint8_t br_##N(uint16_t x) {           \
        marks = 0;                                                            \
        if (x < C) mark(1);                                                   \
        if (x <= C) mark(2);                                                  \
        if (x > C) mark(4);                                                   \
        if (x >= C) mark(8);                                                  \
        if (C < x) mark(16);                                                  \
        if (C >= x) mark(32);                                                 \
        return marks;                                                         \
    }

#define ONE(N, P, E)                                                          \
    __attribute__((noinline)) static uint8_t P##_##N(uint16_t x) { return E; }
#define VALUE(N, C)                                                           \
    ONE(N, lt, x < C)                                                         \
    ONE(N, le, x <= C)                                                        \
    ONE(N, gt, x > C)                                                         \
    ONE(N, ge, x >= C)                                                        \
    static uint8_t val_##N(uint16_t x) {                                      \
        return (uint8_t)(lt_##N(x) | le_##N(x) << 1 | gt_##N(x) << 2 |       \
                         ge_##N(x) << 3 | gt_##N(x) << 4 | le_##N(x) << 5);  \
    }

#define BOTH(N, C) BRANCH(N, C) VALUE(N, C)
BOTH(c0001, 0x0001u)
BOTH(c00ff, 0x00FFu)
BOTH(c0100, 0x0100u)
BOTH(c0101, 0x0101u)
BOTH(c12ff, 0x12FFu)
BOTH(c1300, 0x1300u)
BOTH(cac00, 0xAC00u)
BOTH(cff00, 0xFF00u)
BOTH(cfffe, 0xFFFEu)

static volatile uint32_t wide;

static uint8_t expected(uint16_t x, uint16_t c) {
    uint32_t w;
    uint8_t lt, gt;
    wide = x;
    w = wide;
    lt = w < c;
    gt = w > c;
    return (uint8_t)(lt | (!gt) << 1 | gt << 2 | (!lt) << 3 | gt << 4 |
                     (!gt) << 5);
}

static const uint16_t steps[] = {0x0000, 0x0001, 0x00FF, 0x0100, 0xFF00,
                                 0xFFFF};

int main(void) {
    uint16_t status = 0;
    uint16_t bad_br = 0, bad_val = 0;
    uint8_t k, j;
    static const uint16_t cs[] = {0x0001, 0x00FF, 0x0100, 0x0101, 0x12FF,
                                  0x1300, 0xAC00, 0xFF00, 0xFFFE};

    for (k = 0; k < sizeof(cs) / sizeof(cs[0]); k++) {
        for (j = 0; j < sizeof(steps) / sizeof(steps[0]); j++) {
            uint16_t x;
            uint8_t s;
            for (s = 0; s < 2; s++) {
                x = s ? (uint16_t)(cs[k] + steps[j]) : (uint16_t)(cs[k] - steps[j]);
                switch (k) {
                case 0:
                    bad_br += br_c0001(x) != expected(x, 0x0001u);
                    bad_val += val_c0001(x) != expected(x, 0x0001u);
                    break;
                case 1:
                    bad_br += br_c00ff(x) != expected(x, 0x00FFu);
                    bad_val += val_c00ff(x) != expected(x, 0x00FFu);
                    break;
                case 2:
                    bad_br += br_c0100(x) != expected(x, 0x0100u);
                    bad_val += val_c0100(x) != expected(x, 0x0100u);
                    break;
                case 3:
                    bad_br += br_c0101(x) != expected(x, 0x0101u);
                    bad_val += val_c0101(x) != expected(x, 0x0101u);
                    break;
                case 4:
                    bad_br += br_c12ff(x) != expected(x, 0x12FFu);
                    bad_val += val_c12ff(x) != expected(x, 0x12FFu);
                    break;
                case 5:
                    bad_br += br_c1300(x) != expected(x, 0x1300u);
                    bad_val += val_c1300(x) != expected(x, 0x1300u);
                    break;
                case 6:
                    bad_br += br_cac00(x) != expected(x, 0xAC00u);
                    bad_val += val_cac00(x) != expected(x, 0xAC00u);
                    break;
                case 7:
                    bad_br += br_cff00(x) != expected(x, 0xFF00u);
                    bad_val += val_cff00(x) != expected(x, 0xFF00u);
                    break;
                default:
                    bad_br += br_cfffe(x) != expected(x, 0xFFFEu);
                    bad_val += val_cfffe(x) != expected(x, 0xFFFEu);
                    break;
                }
            }
        }
    }
    if (bad_br == 0) status |= 1;
    if (bad_val == 0) status |= 2;

    return status; /* expect 0x0003 */
}
