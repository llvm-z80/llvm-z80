// RUN: %cc -O2 -S -emit-llvm %s -o - | %llc -O2 -o - | FileCheck %s
// RUN: %cc %s -o %t
// RUN: %run --expect-result=0x000F %t

// CHECK-LABEL: _and_from_ptr:
// CHECK:       and (hl)
// CHECK:       ret

// CHECK-LABEL: _xor_from_ptr:
// CHECK:       xor (hl)
// CHECK:       ret

// CHECK-LABEL: _add_from_ptr:
// CHECK:       add a,(hl)
// CHECK:       ret

// CHECK-LABEL: _sub_from_ptr:
// CHECK:       sub (hl)
// CHECK:       ret

/* Test byte-wise AND, XOR, ADD, SUB folded into memory-direct (HL) forms.
 * Verifies correctness across pointers, globals, and commutative orders. */

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

__attribute__((noinline))
uint8_t and_from_ptr(const uint8_t *p, uint8_t x) {
    return *p & x;
}

__attribute__((noinline))
uint8_t xor_from_ptr(const uint8_t *p, uint8_t x) {
    return x ^ *p; // Commuted order
}

__attribute__((noinline))
uint8_t add_from_ptr(const uint8_t *p, uint8_t x) {
    return *p + x;
}

__attribute__((noinline))
uint8_t sub_from_ptr(uint8_t x, const uint8_t *p) {
    return x - *p; // Only RHS can fold for SUB
}

int main(void) {
    const uint8_t data[] = {0xF0, 0x55, 0x12, 0x05};
    uint16_t status = 0;

    /* Bit 0: AND (HL) -> 0xF0 & 0x3C = 0x30 */
    if (and_from_ptr(&data[0], 0x3C) == 0x30)
        status |= 1;

    /* Bit 1: XOR (HL) -> 0x55 ^ 0xAA = 0xFF */
    if (xor_from_ptr(&data[1], 0xAA) == 0xFF)
        status |= 2;

    /* Bit 2: ADD (HL) -> 0x12 + 0x24 = 0x36 */
    if (add_from_ptr(&data[2], 0x24) == 0x36)
        status |= 4;

    /* Bit 3: SUB (HL) -> 0x20 - 0x05 = 0x1B */
    if (sub_from_ptr(0x20, &data[3]) == 0x1B)
        status |= 8;

    return status; /* expect 0x000F */
}
