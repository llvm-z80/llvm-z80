/* Test 06: i64 bitwise - shifts, AND/OR/XOR, negation, sign extension, assembly */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef long int32_t;
typedef unsigned long long uint64_t;
typedef long long int64_t;

int main() {
    uint16_t status = 0;

    /* Bit 0: i64 shift left across 32-bit boundary:
       1LL<<32 == 0x100000000, 0xFFLL<<28 == 0xFF0000000 */
    {
        volatile uint64_t a = 1;
        volatile uint64_t b = 0xFF;
        uint64_t r1 = a << 32;
        uint64_t r2 = b << 28;
        if (r1 == 0x100000000ULL && r2 == 0xFF0000000ULL)
            status |= (1 << 0);
    }

    /* Bit 1: i64 shift right:
       0x123456789ABCDEF0ULL>>32 == 0x12345678,
       >>48 == 0x1234 */
    {
        volatile uint64_t a = 0x123456789ABCDEF0ULL;
        uint64_t r1 = a >> 32;
        uint64_t r2 = a >> 48;
        if (r1 == 0x12345678ULL && r2 == 0x1234ULL)
            status |= (1 << 1);
    }

    /* Bit 2: i64 AND/OR:
       (0xAAAAAAAA55555555ULL & 0xFF00FF00FF00FF00ULL) == 0xAA00AA0055005500ULL */
    {
        volatile uint64_t a = 0xAAAAAAAA55555555ULL;
        volatile uint64_t b = 0xFF00FF00FF00FF00ULL;
        uint64_t r = a & b;
        if (r == 0xAA00AA0055005500ULL)
            status |= (1 << 2);
    }

    /* Bit 3: signed i64 negation and sign extension:
       -1LL == 0xFFFFFFFFFFFFFFFF;
       -(100000LL) == -100000LL;
       (int64_t)(int32_t)-1 == -1LL */
    {
        volatile int64_t a = -1LL;
        volatile int64_t b = 100000LL;
        int64_t neg_b = -b;
        volatile int32_t c = -1L;
        int64_t ext = (int64_t)c;
        if ((uint64_t)a == 0xFFFFFFFFFFFFFFFFULL &&
            neg_b == -100000LL &&
            ext == -1LL)
            status |= (1 << 3);
    }

    /* Bit 4: i64 XOR self == 0, XOR with all-ones == NOT */
    {
        volatile uint64_t a = 0x123456789ABCDEF0ULL;
        volatile uint64_t allones = 0xFFFFFFFFFFFFFFFFULL;
        uint64_t not_a = a ^ allones;
        /* ~0x123456789ABCDEF0 = 0xEDCBA9876543210F */
        if ((a ^ a) == 0 &&
            (uint32_t)(not_a >> 32) == 0xEDCBA987UL &&
            (uint32_t)not_a == 0x6543210FUL)
            status |= (1 << 4);
    }

    /* Bit 5: i64 OR assembles from halves */
    {
        volatile uint32_t hi = 0xDEADBEEFUL, lo = 0xCAFEBABEUL;
        uint64_t assembled = ((uint64_t)hi << 32) | lo;
        if ((uint32_t)(assembled >> 32) == 0xDEADBEEFUL &&
            (uint32_t)assembled == 0xCAFEBABEUL)
            status |= (1 << 5);
    }

    /* Bit 6: Arithmetic right shift preserves sign:
       (int64_t)(-256LL) >> 4 == -16LL */
    {
        volatile int64_t a = -256LL;
        int64_t r = a >> 4;
        if (r == -16LL) status |= (1 << 6);
    }

    /* Bit 7: i64 shift by 1: left == *2, right == /2 */
    {
        volatile uint64_t a = 0x4000000000000000ULL;
        uint64_t l = a << 1;
        uint64_t r = a >> 1;
        if (l == 0x8000000000000000ULL && r == 0x2000000000000000ULL)
            status |= (1 << 7);
    }

    return status; /* expect 0x00FF */
}
