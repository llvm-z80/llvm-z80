/* Test 05: i64 arithmetic - add, sub, multiply, division */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef long int32_t;
typedef unsigned long long uint64_t;
typedef long long int64_t;

int main() {
    uint16_t status = 0;

    /* Bit 0: i64 add with carry propagation:
       0x00000000FFFFFFFF + 1 == 0x0000000100000000 */
    {
        volatile uint64_t a = 0x00000000FFFFFFFFULL;
        volatile uint64_t b = 1;
        uint64_t r = a + b;
        if (r == 0x0000000100000000ULL) status |= (1 << 0);
    }

    /* Bit 1: i64 subtract across 32-bit boundary:
       0x0000000100000000 - 1 == 0x00000000FFFFFFFF */
    {
        volatile uint64_t a = 0x0000000100000000ULL;
        volatile uint64_t b = 1;
        uint64_t r = a - b;
        if (r == 0x00000000FFFFFFFFULL) status |= (1 << 1);
    }

    /* Bit 2: i64 multiply: 100000LL * 100000LL == 10000000000LL */
    {
        volatile uint64_t a = 100000ULL;
        volatile uint64_t b = 100000ULL;
        uint64_t r = a * b;
        if (r == 10000000000ULL) status |= (1 << 2);
    }

    /* Bit 3: i64 division: 10000000000 / 100000 == 100000; mod == 0 */
    {
        volatile uint64_t a = 10000000000ULL;
        volatile uint64_t b = 100000ULL;
        uint64_t q = a / b;
        uint64_t m = a % b;
        if (q == 100000ULL && m == 0) status |= (1 << 3);
    }

    /* Bit 4: Signed i64: (-1LL) + (-1LL) == -2LL */
    {
        volatile int64_t a = -1LL, b = -1LL;
        if (a + b == -2LL) status |= (1 << 4);
    }

    /* Bit 5: i64 overflow wrap: 0xFFFFFFFFFFFFFFFF + 1 == 0 */
    {
        volatile uint64_t a = 0xFFFFFFFFFFFFFFFFULL;
        uint64_t r = a + 1;
        if (r == 0) status |= (1 << 5);
    }

    /* Bit 6: i64 multiply small values: 12345LL * 67890LL == 838102050LL */
    {
        volatile uint64_t a = 12345ULL, b = 67890ULL;
        if (a * b == 838102050ULL) status |= (1 << 6);
    }

    /* Bit 7: i64 chained sub: 0x100000000 - 0x80000000 - 0x80000000 == 0 */
    {
        volatile uint64_t a = 0x100000000ULL;
        volatile uint64_t b = 0x80000000ULL;
        uint64_t r = a - b - b;
        if (r == 0) status |= (1 << 7);
    }

    return status; /* expect 0x00FF */
}
