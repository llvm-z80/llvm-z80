/* Benchmark: 8-bit rotation operations.
 * Tests native Z80 rotate instruction usage (RLCA, RRCA).
 * expect 0x000F
 */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

/* --- Constant rotation amounts --- */
uint8_t rotl1(uint8_t x) { return (x << 1) | (x >> 7); }
uint8_t rotl2(uint8_t x) { return (x << 2) | (x >> 6); }
uint8_t rotl3(uint8_t x) { return (x << 3) | (x >> 5); }
uint8_t rotr1(uint8_t x) { return (x >> 1) | (x << 7); }
uint8_t rotr2(uint8_t x) { return (x >> 2) | (x << 6); }
uint8_t rotr3(uint8_t x) { return (x >> 3) | (x << 5); }

/* --- Variable rotation amount --- */
uint8_t rotl_var(uint8_t x, uint8_t n) {
    return (x << n) | (x >> (8 - n));
}

uint8_t rotr_var(uint8_t x, uint8_t n) {
    return (x >> n) | (x << (8 - n));
}

/* --- Practical: simple CRC-like hash using rotations --- */
uint8_t rot_hash(const uint8_t *data, uint8_t len) {
    uint8_t h = 0xFF;
    for (uint8_t i = 0; i < len; i++) {
        h ^= data[i];
        h = (h << 3) | (h >> 5); /* rotl by 3 */
        h ^= data[i];
        h = (h >> 2) | (h << 6); /* rotr by 2 */
    }
    return h;
}

/* --- Practical: bit-field rotation for flags manipulation --- */
uint8_t rotate_flags(uint8_t flags, uint8_t positions) {
    /* Rotate flags register by variable amount */
    uint8_t r = (flags << positions) | (flags >> (8 - positions));
    return r;
}

int main() {
    uint16_t status = 0;

    /* Bit 0: constant rotations */
    {
        uint8_t v = 0xA5; /* 10100101 */
        uint8_t r1 = rotl1(v);  /* 01001011 = 0x4B */
        uint8_t r2 = rotr1(v);  /* 11010010 = 0xD2 */
        uint8_t r3 = rotl3(v);  /* 00101101 = 0x2D */
        uint8_t r4 = rotr2(v);  /* 01101001 = 0x69 */
        if (r1 == 0x4B && r2 == 0xD2 && r3 == 0x2D && r4 == 0x69)
            status |= 1;
    }

    /* Bit 1: variable rotations */
    {
        uint8_t v = 0x3C; /* 00111100 */
        uint8_t r1 = rotl_var(v, 1); /* 01111000 = 0x78 */
        uint8_t r2 = rotl_var(v, 4); /* 11000011 = 0xC3 */
        uint8_t r3 = rotr_var(v, 2); /* 00001111 = 0x0F */
        if (r1 == 0x78 && r2 == 0xC3 && r3 == 0x0F)
            status |= 2;
    }

    /* Bit 2: rotation hash */
    {
        uint8_t data1[4];
        data1[0] = 'A'; data1[1] = 'B'; data1[2] = 'C'; data1[3] = 'D';
        uint8_t h1 = rot_hash(data1, 4);
        uint8_t h2 = rot_hash(data1, 4);
        /* Hash should be deterministic and non-trivial */
        if (h1 == h2 && h1 != 0xFF && h1 != 0x00)
            status |= 4;
    }

    /* Bit 3: rotl2 + rotr3 compose correctly */
    {
        uint8_t v = 0x81; /* 10000001 */
        uint8_t r = rotl2(rotr3(v));  /* rotr3: 00110000 = 0x30, rotl2: 11000000 = 0xC0 */
        /* rotr3(0x81) = 0x30, rotl2(0x30) = 0xC0 */
        /* rotl2(rotr3(x)) = rotr1(x) since 3-2=1 */
        uint8_t expected = rotr1(v);  /* 11000000 = 0xC0 */
        if (r == expected)
            status |= 8;
    }

    return status; /* expect 0x000F */
}
