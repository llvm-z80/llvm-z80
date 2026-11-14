// REQUIRES: z80, elf
// RUN: %cc -O2 -S -emit-llvm %s -o - | %llc -O2 -o - | FileCheck %s
// RUN: %cc %s -o %t
// RUN: %run --expect-result=0x003F %t

// CHECK-LABEL: _inc_at:
// CHECK:       inc (hl)
// CHECK:       ret

// CHECK-LABEL: _dec_at:
// CHECK:       dec (hl)
// CHECK:       ret

/* Test INC (HL) / DEC (HL) fold on pointer-based counter updates. */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

__attribute__((noinline)) static void inc_at(uint8_t *p) { (*p)++; }
__attribute__((noinline)) static void dec_at(uint8_t *p) { (*p)--; }
__attribute__((noinline)) static void add1(uint8_t *p) { *p = *p + 1; }
__attribute__((noinline)) static void sub1(uint8_t *p) { *p = *p - 1; }

static uint8_t counter;

int main(void) {
    uint16_t status = 0;
    uint8_t cell;

    cell = 0x40;
    inc_at(&cell);
    if (cell == 0x41) status |= 1;

    cell = 0x00;
    dec_at(&cell);
    if (cell == 0xFF) status |= 2;

    cell = 0x7F;
    add1(&cell);
    if (cell == 0x80) status |= 4;

    cell = 0x80;
    sub1(&cell);
    if (cell == 0x7F) status |= 8;

    counter = 10;
    inc_at(&counter);
    inc_at(&counter);
    dec_at(&counter);
    if (counter == 11) status |= 0x10;

    /* Wrap-around */
    cell = 0xFF;
    inc_at(&cell);
    if (cell == 0x00) status |= 0x20;

    return status; /* expect 0x003F */
}
