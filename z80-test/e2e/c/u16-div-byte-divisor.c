// RUN: %cc %s -o %t
// RUN: %run --expect-result=0x00FF %t

/* Unsigned 16-bit division, modulo, and fused quotient and remainder by a
 * divisor that fits in a byte. Divisors 127 and 128 keep the remainder below
 * 128, and divisors from 129 to 255 let the remainder reach 128. */
typedef unsigned short uint16_t;

__attribute__((noinline)) static uint16_t udiv(uint16_t a, uint16_t b) {
    return a / b;
}

__attribute__((noinline)) static uint16_t umod(uint16_t a, uint16_t b) {
    return a % b;
}

__attribute__((noinline)) static void udivrem(uint16_t a, uint16_t b,
                                              uint16_t *q, uint16_t *r) {
    *q = a / b;
    *r = a % b;
}

__attribute__((noinline)) static int check(uint16_t a, uint16_t b, uint16_t q,
                                           uint16_t r) {
    uint16_t fq, fr;
    udivrem(a, b, &fq, &fr);
    return udiv(a, b) == q && umod(a, b) == r && fq == q && fr == r;
}

int main() {
    int status = 0;

    if (check(65535, 127, 516, 3))
        status |= 1;
    if (check(65535, 128, 511, 127))
        status |= 2;
    if (check(256, 129, 1, 127))
        status |= 4;
    if (check(65535, 129, 508, 3))
        status |= 8;
    if (check(1000, 200, 5, 0))
        status |= 16;
    if (check(256, 255, 1, 1))
        status |= 32;
    if (check(65535, 255, 257, 0))
        status |= 64;
    if (check(65534, 255, 256, 254))
        status |= 128;

    return status;
}
