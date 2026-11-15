// RUN: %cc %s -o %t
// RUN: %run --expect-result=0x03FF %t

/* Tail calls that pass the stack arguments through unchanged. */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;

__attribute__((noinline)) uint16_t sum3(uint16_t a, uint16_t b, uint16_t c) {
    return a + 2 * b + 3 * c;
}

__attribute__((noinline)) uint16_t fwd3(uint16_t a, uint16_t b, uint16_t c) {
    return sum3(a, b, c);
}

__attribute__((noinline)) uint16_t swap3(uint16_t a, uint16_t b, uint16_t c) {
    return sum3(b, a, c);
}

__attribute__((noinline)) uint16_t fwd3_twice(uint16_t a, uint16_t b,
                                              uint16_t c) {
    return fwd3(a, b, c);
}

__attribute__((noinline)) uint8_t mix8(uint8_t a, uint8_t b, uint8_t c) {
    return a ^ (b << 1) ^ (c << 2);
}

__attribute__((noinline)) uint8_t fwd8(uint8_t a, uint8_t b, uint8_t c) {
    return mix8(a, b, c);
}

__attribute__((noinline)) uint16_t add32(uint16_t a, uint16_t b, uint32_t c) {
    return a + b + (uint16_t)(c >> 16) + (uint16_t)c;
}

__attribute__((noinline)) uint16_t fwd32(uint16_t a, uint16_t b, uint32_t c) {
    return add32(a, b, c);
}

#define SMALLC __attribute__((noinline, smallc))

SMALLC uint16_t sum3s(uint16_t a, uint16_t b, uint16_t c) {
    return a + 2 * b + 3 * c;
}

SMALLC uint16_t fwd3s(uint16_t a, uint16_t b, uint16_t c) {
    return sum3s(a, b, c);
}

SMALLC uint16_t swap3s(uint16_t a, uint16_t b, uint16_t c) {
    return sum3s(b, a, c);
}

SMALLC uint16_t sum2s(uint16_t b, uint16_t c) { return 2 * b + 3 * c; }

SMALLC uint16_t last2s(uint16_t a, uint16_t b, uint16_t c) {
    return sum2s(b, c);
}

SMALLC uint8_t mix8s(uint8_t a, uint8_t b, uint8_t c) {
    return a ^ (b << 1) ^ (c << 2);
}

SMALLC uint8_t fwd8s(uint8_t a, uint8_t b, uint8_t c) {
    return mix8s(a, b, c);
}

int main() {
    uint16_t status = 0;
    volatile uint16_t a = 1, b = 10, c = 100;

    /* Bit 0: 1 + 20 + 300 */
    if (fwd3(a, b, c) == 321)
        status |= 1;

    /* Bit 1: the register arguments change, the stack argument stays */
    if (swap3(a, b, c) == 312)
        status |= 2;

    /* Bit 2: two jumps in a row */
    if (fwd3_twice(a, b, c) == 321)
        status |= 4;

    /* Bit 3: one-byte stack argument, 1 ^ 4 ^ 16 */
    {
        volatile uint8_t x = 1, y = 2, z = 4;
        if (fwd8(x, y, z) == 21)
            status |= 8;
    }

    /* Bit 4: four-byte stack argument, 1 + 10 + 3 + 4 */
    {
        volatile uint32_t w = 0x00030004;
        if (fwd32(a, b, w) == 18)
            status |= 16;
    }

    /* Bit 5: the stack stays balanced over many calls */
    {
        uint16_t acc = 0;
        for (volatile uint16_t i = 0; i < 300; i++)
            acc += fwd3(i, b, c);
        if (acc == 9778)
            status |= 32;
    }

    /* Bit 6: __smallc pushes left to right */
    if (fwd3s(a, b, c) == 321)
        status |= 64;

    /* Bit 7: swapped __smallc arguments stay a call */
    if (swap3s(a, b, c) == 312)
        status |= 128;

    /* Bit 8: the last two __smallc arguments stay in place */
    if (last2s(a, b, c) == 320)
        status |= 256;

    /* Bit 9: one-byte __smallc stack arguments */
    {
        volatile uint8_t x = 1, y = 2, z = 4;
        if (fwd8s(x, y, z) == 21)
            status |= 512;
    }

    return status;
}
