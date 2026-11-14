// REQUIRES: z80, elf
// RUN: %cc -O2 -S -emit-llvm %s -o - | %llc -O2 -o - | FileCheck %s
// RUN: %cc %s -o %t
// RUN: %run --expect-result=0x0007 %t

// CHECK-LABEL: _cmp_blocks:
// CHECK:       dec c
// CHECK-DAG:   inc de
// CHECK-DAG:   inc hl
// CHECK:       jr nz,
// CHECK:       ret

/* Test LSR cost-model optimizations and narrow loop down-counters.
 * Verifies that pointer comparison loops and countdown loops execute
 * with exact correctness under all optimization levels. */

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

__attribute__((noinline))
uint8_t cmp_blocks(const uint8_t *a, const uint8_t *b, uint8_t len) {
    while (len--) {
        if (*a != *b)
            return 1;
        a++;
        b++;
    }
    return 0;
}

static const uint8_t pat1[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
static const uint8_t pat2[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
static const uint8_t pat3[8] = { 1, 2, 3, 4, 5, 9, 7, 8 };

int main(void) {
    uint16_t status = 0;

    /* Bit 0: identical patterns return 0 */
    if (cmp_blocks(pat1, pat2, 8) == 0)
        status |= 1;

    /* Bit 1: differing patterns return 1 */
    if (cmp_blocks(pat1, pat3, 8) == 1)
        status |= 2;

    /* Bit 2: identical prefix (5 bytes) return 0 */
    if (cmp_blocks(pat1, pat3, 5) == 0)
        status |= 4;

    return status; /* expect 0x0007 */
}
