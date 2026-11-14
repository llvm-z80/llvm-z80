// RUN: %cc -O2 -S -emit-llvm %s -o - | %llc -O2 -o - | FileCheck %s
// RUN: %cc %s -o %t
// RUN: %run --expect-result=0x000F %t

// CHECK-LABEL: _dec_ptr_branch_nonzero:
// CHECK:       dec (hl)
// CHECK-NEXT:  ret z

// CHECK-LABEL: _dec_global_branch_zero:
// CHECK:       ld hl,_counter
// CHECK-NEXT:  dec (hl)
// CHECK-NEXT:  ret nz

/* Test compare-against-zero combined with memory DEC/INC (HL).
 * Verifies that countdown loops and zero-branching correctly consume
 * the Z-flag set directly by DEC/INC (HL). */

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

uint8_t counter;

__attribute__((noinline))
void dec_ptr_branch_nonzero(uint8_t *p, void (*target)(void)) {
    if (--(*p) != 0)
        target();
}

__attribute__((noinline))
void dec_global_branch_zero(void (*target)(void)) {
    if (--counter == 0)
        target();
}

static uint16_t side_effect = 0;
static void hit_nonzero(void) { side_effect |= 1; }
static void hit_zero(void) { side_effect |= 2; }

__attribute__((noinline))
static uint16_t countdown(uint8_t *p) {
    uint16_t iterations = 0;
    while (*p) {
        (*p)--;
        iterations++;
    }
    return iterations;
}

__attribute__((noinline))
static uint8_t dec_and_check_zero(uint8_t *p) {
    if (--(*p) == 0)
        return 1;
    return 0;
}

__attribute__((noinline))
static uint8_t dec_and_check_nonzero(uint8_t *p) {
    if (--(*p) != 0)
        return 1;
    return 0;
}

__attribute__((noinline))
static uint8_t inc_and_check_zero(uint8_t *p) {
    if (++(*p) == 0)
        return 1;
    return 0;
}

int main(void) {
    uint16_t status = 0;

    /* Bit 0: countdown loop from 5 to 0 */
    {
        uint8_t n = 5;
        if (countdown(&n) == 5 && n == 0)
            status |= 1;
    }

    /* Bit 1: decrement to zero returns 1 */
    {
        uint8_t val = 1;
        if (dec_and_check_zero(&val) == 1 && val == 0)
            status |= 2;
    }

    /* Bit 2: decrement to non-zero returns 1 */
    {
        uint8_t val = 3;
        if (dec_and_check_nonzero(&val) == 1 && val == 2)
            status |= 4;
    }

    /* Bit 3: increment wrap to zero (0xFF + 1 = 0) */
    {
        uint8_t val = 0xFF;
        if (inc_and_check_zero(&val) == 1 && val == 0)
            status |= 8;
    }

    /* Also verify branched side effects */
    {
        uint8_t val = 2;
        dec_ptr_branch_nonzero(&val, hit_nonzero);
        dec_ptr_branch_nonzero(&val, hit_nonzero); // val becomes 0, must not hit
        counter = 1;
        dec_global_branch_zero(hit_zero);
        if (side_effect != 3)
            return 0;
    }

    return status; /* expect 0x000F */
}
