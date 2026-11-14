// REQUIRES: z80, elf
// RUN: %cc -O2 -S -emit-llvm %s -o - | %llc -O2 -o - | FileCheck %s
// RUN: %cc %s -o %t
// RUN: %run --expect-result=0x000F %t

// CHECK-LABEL: _ub_lt:
// CHECK:       cp (hl)
// CHECK:       ret

// CHECK-LABEL: _ub_ge:
// CHECK:       cp (hl)
// CHECK:       ret

/* A byte compared with a byte in memory, in branches. The compiler folds the
 * load into CP (HL) when the compare only feeds a branch and the loaded byte
 * has no other use; this checks that the folded compares agree with the
 * widened 16-bit reference for every pair of edge values, unsigned and
 * signed, and with a pointer walk that compares each byte with a sentinel. */
typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short uint16_t;

/* Branch forms: each returns the outcome of one comparison through a branch
 * on the flags, with the right-hand byte read through a pointer. */
#define UNSIGNED_BRANCH(NAME, OP)                                             \
    __attribute__((noinline)) static uint8_t NAME(uint8_t a, const uint8_t *p) { \
        if (a OP *p)                                                          \
            return 1;                                                         \
        return 0;                                                             \
    }
UNSIGNED_BRANCH(ub_eq, ==)
UNSIGNED_BRANCH(ub_ne, !=)
UNSIGNED_BRANCH(ub_lt, <)
UNSIGNED_BRANCH(ub_le, <=)
UNSIGNED_BRANCH(ub_gt, >)
UNSIGNED_BRANCH(ub_ge, >=)

#define SIGNED_BRANCH(NAME, OP)                                               \
    __attribute__((noinline)) static uint8_t NAME(int8_t a, const int8_t *p) { \
        if (a OP *p)                                                          \
            return 1;                                                         \
        return 0;                                                             \
    }
SIGNED_BRANCH(sb_eq, ==)
SIGNED_BRANCH(sb_lt, <)
SIGNED_BRANCH(sb_le, <=)
SIGNED_BRANCH(sb_gt, >)
SIGNED_BRANCH(sb_ge, >=)

/* Reference: the same comparison on values widened to 16 bits. The widened
 * values go through volatile objects so the compiler cannot fold the
 * reference back into the byte compare under test. */
static volatile uint16_t wa, wb;
static volatile int sa, sb;

static const uint8_t edges[] = {0x00, 0x01, 0x7F, 0x80, 0xFF};

int main(void) {
    uint16_t status = 0;
    uint16_t bad_u = 0, bad_s = 0;
    uint8_t i, j;

    for (i = 0; i < sizeof(edges); i++) {
        for (j = 0; j < sizeof(edges); j++) {
            uint8_t a = edges[i];
            uint8_t b = edges[j];
            int x, y;

            wa = a;
            wb = b;
            bad_u += ub_eq(a, &b) != (wa == wb);
            bad_u += ub_ne(a, &b) != (wa != wb);
            bad_u += ub_lt(a, &b) != (wa < wb);
            bad_u += ub_le(a, &b) != (wa <= wb);
            bad_u += ub_gt(a, &b) != (wa > wb);
            bad_u += ub_ge(a, &b) != (wa >= wb);

            /* Sign-extend by hand: 0x80..0xFF are negative. */
            x = a >= 0x80 ? (int)a - 256 : (int)a;
            y = b >= 0x80 ? (int)b - 256 : (int)b;
            sa = x;
            sb = y;
            bad_s += sb_eq((int8_t)a, (const int8_t *)&b) != (sa == sb);
            bad_s += sb_lt((int8_t)a, (const int8_t *)&b) != (sa < sb);
            bad_s += sb_le((int8_t)a, (const int8_t *)&b) != (sa <= sb);
            bad_s += sb_gt((int8_t)a, (const int8_t *)&b) != (sa > sb);
            bad_s += sb_ge((int8_t)a, (const int8_t *)&b) != (sa >= sb);
        }
    }
    if (bad_u == 0)
        status |= 1;
    if (bad_s == 0)
        status |= 2;

    /* Pointer walk: count the bytes equal to a sentinel. Expected 4 (the
     * sentinel is 2) and, for a second sentinel that is absent, 0. */
    {
        static const uint8_t buf[] = {1, 2, 3, 2, 2, 0xFF, 2, 0x80};
        uint8_t sentinel = 2, absent = 0x42;
        uint8_t hits = 0, misses = 0;
        const uint8_t *p;
        for (p = buf; p != buf + sizeof(buf); p++) {
            if (sentinel == *p)
                hits++;
            if (absent == *p)
                misses++;
        }
        if (hits == 4)
            status |= 4;
        if (misses == 0)
            status |= 8;
    }

    return status;
}
