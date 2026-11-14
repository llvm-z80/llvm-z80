// REQUIRES: z80, elf
// RUN: %cc -O2 -S -emit-llvm %s -o - | %llc -O2 -o - | FileCheck %s
// RUN: %cc %s -o %t
// RUN: %run --expect-result=0x0007 %t

// CHECK-LABEL: _get_row_ptr:
// CHECK:       add hl,hl
// CHECK:       ld bc,_grid
// CHECK-NEXT:  add hl,bc
// CHECK:       ret

/* Test 16-bit commutative pointer and integer additions where an operand
 * is produced in HL (e.g. shifted array index).
 * Verifies that swapping operands preserves correct results across
 * various address calculations and array accesses. */

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

const uint8_t grid[4][8] = {
    {  0,  1,  2,  3,  4,  5,  6,  7 },
    { 10, 11, 12, 13, 14, 15, 16, 17 },
    { 20, 21, 22, 23, 24, 25, 26, 27 },
    { 30, 31, 32, 33, 34, 35, 36, 37 }
};

__attribute__((noinline))
static uint8_t get_element(uint8_t row, uint8_t col) {
    return grid[row][col];
}

__attribute__((noinline))
static uint16_t sum_row(uint8_t row) {
    uint16_t sum = 0;
    for (uint8_t i = 0; i < 8; ++i)
        sum += grid[row][i];
    return sum;
}

__attribute__((noinline))
const uint8_t *get_row_ptr(uint8_t row) {
    return &grid[row][0];
}

int main(void) {
    uint16_t status = 0;

    /* Bit 0: element lookup grid[2][5] == 25 */
    if (get_element(2, 5) == 25 && get_element(0, 0) == 0 && get_element(3, 7) == 37)
        status |= 1;

    /* Bit 1: sum of row 1 (10+11+12+13+14+15+16+17 = 108) */
    if (sum_row(1) == 108 && sum_row(2) == 188)
        status |= 2;

    /* Bit 2: row pointer offset check */
    {
        const uint8_t *p = get_row_ptr(3);
        if (p[0] == 30 && p[4] == 34)
            status |= 4;
    }

    return status; /* expect 0x0007 */
}
