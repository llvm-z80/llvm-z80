/* Test 48: Dynamic stack allocation (alloca / VLA) */
/* Tests that dynamic alloca works correctly with frame pointer */
/* SKIP-IF: sm83 */
#include <alloca.h>
typedef unsigned char u8;
typedef unsigned short u16;

volatile u8 zero = 0; /* prevent constant folding */

/* --- Bit 0: basic alloca --- */
__attribute__((noinline))
u16 test_alloca_basic(u16 n) {
    u8 *buf = (u8 *)alloca(n);
    buf[0] = 10;
    buf[n - 1] = 20;
    return (u16)buf[0] + (u16)buf[n - 1];
}

/* --- Bit 1: alloca with local vars before and after --- */
__attribute__((noinline))
u16 test_alloca_locals(u16 n) {
    volatile u16 before = 0xAA;
    u8 *buf = (u8 *)alloca(n);
    volatile u16 after = 0xBB;
    buf[0] = 5;
    buf[1] = 15;
    /* Verify locals survive alloca */
    return before + after + (u16)buf[0] + (u16)buf[1];
    /* 0xAA + 0xBB + 5 + 15 = 170 + 187 + 20 = 377 */
}

/* --- Bit 2: VLA (variable-length array) --- */
__attribute__((noinline))
u16 test_vla(u16 n) {
    u8 arr[n];
    u8 i;
    for (i = 0; i < n && i < 5; i++) {
        arr[i] = i + 1;
    }
    /* sum of 1..5 = 15 */
    u16 sum = 0;
    for (i = 0; i < n && i < 5; i++) {
        sum += arr[i];
    }
    return sum;
}

/* --- Bit 3: multiple alloca calls in same function --- */
__attribute__((noinline))
u16 test_alloca_multi(u16 n1, u16 n2) {
    u8 *buf1 = (u8 *)alloca(n1);
    u8 *buf2 = (u8 *)alloca(n2);
    buf1[0] = 100;
    buf2[0] = 50;
    /* Verify both buffers are independent */
    return (u16)buf1[0] + (u16)buf2[0];
}

u16 main(void) {
    u16 status = 0;
    u16 n;

    /* Bit 0: basic alloca */
    n = 8 + zero;
    if (test_alloca_basic(n) == 30)
        status |= 1;

    /* Bit 1: alloca with locals */
    n = 4 + zero;
    if (test_alloca_locals(n) == 377)
        status |= 2;

    /* Bit 2: VLA */
    n = 5 + zero;
    if (test_vla(n) == 15)
        status |= 4;

    /* Bit 3: multiple alloca */
    n = 4 + zero;
    if (test_alloca_multi(n, n) == 150)
        status |= 8;

    return status;
}
