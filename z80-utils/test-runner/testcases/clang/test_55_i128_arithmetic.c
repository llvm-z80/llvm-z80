/* Test 55: i128 arithmetic - add, sub, multiply, division, modulo */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef unsigned long long uint64_t;
typedef long long int64_t;
typedef __uint128_t uint128_t;
typedef __int128_t int128_t;

int main() {
    uint16_t status = 0;

    /* Bit 0: i128 add with carry propagation across 64-bit boundary:
       0x0000...00FFFFFFFFFFFFFFFF + 1 == 0x0000...0100000000_00000000 */
    {
        volatile uint128_t a = (uint128_t)0xFFFFFFFFFFFFFFFFULL;
        volatile uint128_t b = 1;
        uint128_t r = a + b;
        if (r == ((uint128_t)1 << 64)) status |= (1 << 0);
    }

    /* Bit 1: i128 subtract across 64-bit boundary:
       0x0000...0100000000_00000000 - 1 == 0x0000...00FFFFFFFFFFFFFFFF */
    {
        volatile uint128_t a = (uint128_t)1 << 64;
        volatile uint128_t b = 1;
        uint128_t r = a - b;
        if (r == (uint128_t)0xFFFFFFFFFFFFFFFFULL) status |= (1 << 1);
    }

    /* Bit 2: i128 multiply: 0x100000000 * 0x100000000 == 1 << 64 */
    {
        volatile uint128_t a = 0x100000000ULL;
        volatile uint128_t b = 0x100000000ULL;
        uint128_t r = a * b;
        if (r == ((uint128_t)1 << 64)) status |= (1 << 2);
    }

    /* Bit 3: i128 unsigned division:
       (1 << 64) / 0x100000000 == 0x100000000 */
    {
        volatile uint128_t a = (uint128_t)1 << 64;
        volatile uint128_t b = 0x100000000ULL;
        uint128_t q = a / b;
        if (q == (uint128_t)0x100000000ULL) status |= (1 << 3);
    }

    /* Bit 4: i128 unsigned modulo:
       ((1 << 64) + 7) % 0x100000000 == 7 */
    {
        volatile uint128_t a = ((uint128_t)1 << 64) + 7;
        volatile uint128_t b = 0x100000000ULL;
        uint128_t m = a % b;
        if (m == 7) status |= (1 << 4);
    }

    /* Bit 5: i128 signed: (-1) + (-1) == -2 */
    {
        volatile int128_t a = -1, b = -1;
        int128_t r = a + b;
        if (r == -2) status |= (1 << 5);
    }

    /* Bit 6: i128 multiply small values: 12345 * 67890 == 838102050 */
    {
        volatile uint128_t a = 12345, b = 67890;
        uint128_t r = a * b;
        if (r == 838102050ULL) status |= (1 << 6);
    }

    /* Bit 7: i128 overflow wrap:
       0xFFFF...FFFF + 1 == 0 */
    {
        volatile uint128_t a = ~(uint128_t)0;
        uint128_t r = a + 1;
        if (r == 0) status |= (1 << 7);
    }

    return status; /* expect 0x00FF */
}
