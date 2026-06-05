/* Test 12: bitwise ops - popcount, bit reverse, flag pack, extract/insert */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

/* Count set bits in a 16-bit value */
uint8_t popcount16(uint16_t val) {
    uint8_t count = 0;
    while (val) {
        count += (val & 1);
        val >>= 1;
    }
    return count;
}

/* Reverse bits of a byte */
uint8_t reverse_byte(uint8_t val) {
    uint8_t rev = 0;
    for (uint8_t i = 0; i < 8; i++) {
        rev = (rev << 1) | (val & 1);
        val >>= 1;
    }
    return rev;
}

/* Pack 8 bools into a byte */
uint8_t pack_flags(uint8_t f0, uint8_t f1, uint8_t f2, uint8_t f3,
                   uint8_t f4, uint8_t f5, uint8_t f6, uint8_t f7) {
    return (f0?1:0) | ((f1?1:0)<<1) | ((f2?1:0)<<2) | ((f3?1:0)<<3) |
           ((f4?1:0)<<4) | ((f5?1:0)<<5) | ((f6?1:0)<<6) | ((f7?1:0)<<7);
}

/* Extract n bits starting at position pos */
uint16_t extract_bits(uint16_t val, uint8_t pos, uint8_t n) {
    return (val >> pos) & ((1 << n) - 1);
}

/* Insert n bits at position pos */
uint16_t insert_bits(uint16_t val, uint16_t bits, uint8_t pos, uint8_t n) {
    uint16_t mask = ((1 << n) - 1) << pos;
    return (val & ~mask) | ((bits << pos) & mask);
}

int main() {
    uint16_t status = 0;

    /* Bit 0: popcount16: popcount(0)==0, popcount(0xFFFF)==16, popcount(0xAAAA)==8 */
    {
        volatile uint16_t v1 = 0, v2 = 0xFFFF, v3 = 0xAAAA;
        if (popcount16(v1) == 0 && popcount16(v2) == 16 && popcount16(v3) == 8)
            status |= (1 << 0);
    }

    /* Bit 1: bit reversal of byte: reverse(0xB2)==0x4D
       0xB2 = 10110010, reversed = 01001101 = 0x4D */
    {
        volatile uint8_t v = 0xB2;
        if (reverse_byte(v) == 0x4D)
            status |= (1 << 1);
    }

    /* Bit 2: flag packing: pack 8 bools into byte, unpack and verify each bit */
    {
        uint8_t r = pack_flags(1, 0, 1, 1, 0, 0, 1, 0);
        /* bits: 01001101 = 0x4D */
        uint8_t ok = 1;
        if (!(r & (1 << 0))) ok = 0;  /* f0=1 */
        if ( (r & (1 << 1))) ok = 0;  /* f1=0 */
        if (!(r & (1 << 2))) ok = 0;  /* f2=1 */
        if (!(r & (1 << 3))) ok = 0;  /* f3=1 */
        if ( (r & (1 << 4))) ok = 0;  /* f4=0 */
        if ( (r & (1 << 5))) ok = 0;  /* f5=0 */
        if (!(r & (1 << 6))) ok = 0;  /* f6=1 */
        if ( (r & (1 << 7))) ok = 0;  /* f7=0 */
        if (ok) status |= (1 << 2);
    }

    /* Bit 3: extract_bits/insert_bits roundtrip:
       extract 3 bits from position 4, insert them back, verify */
    {
        volatile uint16_t orig = 0x5678;
        uint16_t extracted = extract_bits(orig, 4, 3);
        /* 0x5678 = 0101 0110 0111 1000, bits 4-6 = 111 = 7 */
        uint16_t cleared = orig & ~(((uint16_t)7) << 4);
        uint16_t rebuilt = insert_bits(cleared, extracted, 4, 3);
        if (rebuilt == orig) status |= (1 << 3);
    }

    /* Bit 4: popcount single bits: popcount(1)==1, popcount(0x8000)==1 */
    {
        volatile uint16_t a = 1, b = 0x8000;
        if (popcount16(a) == 1 && popcount16(b) == 1)
            status |= (1 << 4);
    }

    /* Bit 5: reverse_byte involution: reverse(reverse(x)) == x */
    {
        volatile uint8_t v1 = 0x37, v2 = 0xA9;
        if (reverse_byte(reverse_byte(v1)) == 0x37 &&
            reverse_byte(reverse_byte(v2)) == 0xA9)
            status |= (1 << 5);
    }

    /* Bit 6: extract high nibble and low nibble, recombine */
    {
        volatile uint16_t val = 0xABCD;
        uint16_t hi = extract_bits(val, 12, 4);  /* 0xA */
        uint16_t lo = extract_bits(val, 0, 4);   /* 0xD */
        if (hi == 0xA && lo == 0xD) status |= (1 << 6);
    }

    /* Bit 7: Pack all-ones and all-zeros, check extremes */
    {
        uint8_t all_set = pack_flags(1,1,1,1,1,1,1,1);
        uint8_t all_clr = pack_flags(0,0,0,0,0,0,0,0);
        if (all_set == 0xFF && all_clr == 0x00) status |= (1 << 7);
    }

    return status; /* expect 0x00FF */
}
