// RUN: %cc %s -o %t
// RUN: %run --expect-result=0x0007 %t

/* Test byte-wise OR folded into OR (HL) from indirect memory load. */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

__attribute__((noinline))
static uint8_t or_from_ptr(volatile uint8_t *ptr, uint8_t value) {
    return *ptr | value;
}

int main(void) {
    volatile uint8_t bytes[] = {0x10, 0xF0, 0x00};
    uint16_t status = 0;

    if (or_from_ptr(&bytes[0], 0x01) == 0x11)
        status |= 1;
    if (or_from_ptr(&bytes[1], 0x0F) == 0xFF)
        status |= 2;
    if (or_from_ptr(&bytes[2], 0x5A) == 0x5A)
        status |= 4;

    return status; /* expect 0x0007 */
}
