/* Benchmark: i32 arithmetic */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef long int32_t;

int main() {
    uint16_t status = 0;

    /* Bit 0: Multiply and divide roundtrip: 12345*6789=83810205 */
    {
        volatile uint32_t a = 12345UL, b = 6789UL;
        uint32_t prod = a * b;
        uint32_t back = prod / b;
        if (prod == 83810205UL && back == 12345UL) status |= 1;
    }

    /* Bit 1: Unsigned div/mod: 1000000/700=1428 rem 400 */
    {
        volatile uint32_t a = 1000000UL, b = 700UL;
        uint32_t q = a / b;
        uint32_t r = a % b;
        if (q == 1428 && r == 400) status |= 2;
    }

    /* Bit 2: Signed div/mod: (-3000000)/30=-100000, rem=0 */
    {
        volatile int32_t a = -3000000L, b = 30;
        int32_t q = a / b;
        int32_t r = a % b;
        if (q == -100000L && r == 0) status |= 4;
    }

    /* Bit 3: Shift and rotate: byte extraction roundtrip */
    {
        volatile uint32_t val = 0xDEADBEEFUL;
        uint8_t b3 = (uint8_t)(val >> 24);
        uint8_t b2 = (uint8_t)(val >> 16);
        uint8_t b1 = (uint8_t)(val >> 8);
        uint8_t b0 = (uint8_t)(val);
        uint32_t rebuilt = ((uint32_t)b3 << 24) | ((uint32_t)b2 << 16) |
                           ((uint32_t)b1 << 8) | b0;
        if (rebuilt == 0xDEADBEEFUL) status |= 8;
    }

    /* Bit 4: Bitwise AND/OR/XOR */
    {
        volatile uint32_t a = 0xFF00FF00UL, b = 0x0FF00FF0UL;
        uint32_t x = a ^ b;  /* 0xF0F0F0F0 */
        uint32_t y = a & b;  /* 0x0F000F00 */
        uint32_t z = a | b;  /* 0xFFF0FFF0 */
        if (x == 0xF0F0F0F0UL && y == 0x0F000F00UL && z == 0xFFF0FFF0UL)
            status |= 0x10;
    }

    /* Bit 5: Add/sub chain: 100000+50000-30000+20000-40000 = 100000 */
    {
        volatile uint32_t a = 100000UL, b = 50000UL;
        volatile uint32_t c = 30000UL, d = 20000UL, e = 40000UL;
        uint32_t r = a + b - c + d - e;
        if (r == 100000UL) status |= 0x20;
    }

    /* Bit 6: Factorial loop: 1*2*...*8 = 40320 */
    {
        volatile uint32_t init = 1;
        uint32_t r = init;
        uint8_t i;
        for (i = 2; i <= 8; i++) r *= (uint32_t)i;
        if (r == 40320UL) status |= 0x40;
    }

    /* Bit 7: Signed negation and carry propagation */
    {
        volatile int32_t a = -123456L;
        volatile uint32_t b = 0xFFFFFFFFUL;
        int32_t neg = -a;
        uint32_t inc = b + 1; /* carry propagation: 0x100000000 truncated to 0 */
        if (neg == 123456L && inc == 0UL) status |= 0x80;
    }

    return status; /* expect 0x00FF */
}
