// RUN: %cc %s -o %t
// RUN: %run --expect-result=0x0003 %t

/* Test static frames with external leaf function calls under assume-no-callbacks.
 * Verifies that functions calling external helpers compute correct results. */

typedef unsigned short uint16_t;

/* External leaf function implemented in another TU or assembly */
__attribute__((noinline))
uint16_t external_leaf(uint16_t x) {
    return x * 3 + 1;
}

__attribute__((noinline))
static uint16_t compute(uint16_t val) {
    volatile uint16_t local_buf[4];
    local_buf[0] = val;
    local_buf[1] = external_leaf(local_buf[0]);
    return local_buf[1];
}

int main(void) {
    uint16_t status = 0;

    /* Bit 0: compute(4) == 4*3+1 == 13 */
    if (compute(4) == 13)
        status |= 1;

    /* Bit 1: compute(10) == 10*3+1 == 31 */
    if (compute(10) == 31)
        status |= 2;

    return status; /* expect 0x0003 */
}
