// REQUIRES: z80, elf
// RUN: %cc -O2 -S -emit-llvm %s -o - | %llc -O2 -o - | FileCheck %s
// RUN: %cc %s -o %t
// RUN: %run --expect-result=0x0007 %t

// CHECK-LABEL: _or_from_ptr:
// CHECK:       or (hl)
// CHECK:       ret

/* Test byte-wise OR folded into OR (HL) from indirect memory load. */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

__attribute__((noinline))
uint8_t or_from_ptr(volatile uint8_t *ptr, uint8_t value) {
    return *ptr | value;
}

int main(void) {
    volatile uint8_t bytes[] = {0x10, 0xF0, 0x00};
    uint16_t status = 0;

    /* Bit 0: 0x10 | 0x01 = 0x11 */
    if (or_from_ptr(&bytes[0], 0x01) == 0x11)
        status |= 1;

    /* Bit 1: 0xF0 | 0x0F = 0xFF */
    if (or_from_ptr(&bytes[1], 0x0F) == 0xFF)
        status |= 2;

    /* Bit 2: 0x00 | 0x5A = 0x5A */
    if (or_from_ptr(&bytes[2], 0x5A) == 0x5A)
        status |= 4;

    return status; /* expect 0x0007 */
}
