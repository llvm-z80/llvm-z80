/* Benchmark: i64 arithmetic */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef unsigned long long uint64_t;
typedef long long int64_t;

int main() {
    uint16_t status = 0;

    /* Bit 0: Multiply and divide roundtrip: 100000*100000=10000000000 */
    {
        volatile uint64_t a = 100000ULL, b = 100000ULL;
        uint64_t prod = a * b;
        uint64_t back = prod / b;
        if (prod == 10000000000ULL && back == 100000ULL) status |= 1;
    }

    /* Bit 1: Unsigned div/mod: 10000000000/700000=14285 rem 500000 */
    {
        volatile uint64_t a = 10000000000ULL, b = 700000ULL;
        uint64_t q = a / b;
        uint64_t r = a % b;
        if (q == 14285 && r == 500000ULL) status |= 2;
    }

    /* Bit 2: Signed div/mod: (-1234567890123)/1000000=-1234567 rem -890123 */
    {
        volatile int64_t a = -1234567890123LL, b = 1000000LL;
        int64_t q = a / b;
        int64_t r = a % b;
        if (q == -1234567LL && r == -890123LL) status |= 4;
    }

    /* Bit 3: Shift across 32-bit boundary: 1<<32, >>32 roundtrip */
    {
        volatile uint64_t a = 1;
        uint64_t shifted = a << 32;
        uint64_t back = shifted >> 32;
        if (shifted == 0x100000000ULL && back == 1) status |= 8;
    }

    /* Bit 4: Bitwise AND/OR/XOR */
    {
        volatile uint64_t a = 0xFF00FF00FF00FF00ULL;
        volatile uint64_t b = 0x0FF00FF00FF00FF0ULL;
        uint64_t x = a ^ b;  /* 0xF0F0F0F0F0F0F0F0 */
        uint64_t y = a & b;  /* 0x0F000F000F000F00 */
        uint64_t z = a | b;  /* 0xFFF0FFF0FFF0FFF0 */
        if (x == 0xF0F0F0F0F0F0F0F0ULL &&
            y == 0x0F000F000F000F00ULL &&
            z == 0xFFF0FFF0FFF0FFF0ULL) status |= 0x10;
    }

    /* Bit 5: Add/sub chain across 32-bit boundary */
    {
        volatile uint64_t a = 0xFFFFFFF0ULL, b = 0x20ULL;
        uint64_t sum = a + b; /* 0x100000010 */
        uint64_t diff = sum - a; /* 0x20 */
        if (sum == 0x100000010ULL && diff == 0x20ULL) status |= 0x20;
    }

    /* Bit 6: Factorial loop: 1*2*...*15 = 1307674368000 */
    {
        volatile uint64_t init = 1;
        uint64_t r = init;
        uint8_t i;
        for (i = 2; i <= 15; i++) r *= (uint64_t)i;
        if (r == 1307674368000ULL) status |= 0x40;
    }

    /* Bit 7: Signed negation and borrow propagation */
    {
        volatile int64_t a = -1234567890123LL;
        volatile uint64_t b = 0x100000000ULL;
        int64_t neg = -a;
        uint64_t dec = b - 1; /* borrow: 0xFFFFFFFF */
        if (neg == 1234567890123LL && dec == 0xFFFFFFFFULL) status |= 0x80;
    }

    return status; /* expect 0x00FF */
}
