/* Test 11: f32/i64 mixed - conversions and widening multiplies */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned long uint32_t;
typedef long int32_t;
typedef unsigned long long uint64_t;
typedef long long int64_t;

int main() {
    uint16_t status = 0;

    /* Bit 0: i32 to float and back: (int32_t)(float)100000 == 100000 */
    {
        volatile int32_t a = 100000L;
        float f = (float)a;
        int32_t b = (int32_t)f;
        if (b == 100000L) status |= (1 << 0);
    }

    /* Bit 1: float to i32: (int32_t)100000.0f == 100000 */
    {
        volatile float f = 100000.0f;
        int32_t i = (int32_t)f;
        if (i == 100000L) status |= (1 << 1);
    }

    /* Bit 2: i64 accumulation in loop: sum i=1..20 as i64 -> 210 */
    {
        volatile uint64_t acc = 0;
        for (uint8_t i = 1; i <= 20; i++)
            acc += (uint64_t)i;
        if (acc == 210ULL) status |= (1 << 2);
    }

    /* Bit 3: i16 * i16 -> i64 widening:
       (int64_t)(int32_t)30000 * (int64_t)(int32_t)30000 == 900000000LL */
    {
        volatile int16_t a = 30000, b = 30000;
        int64_t r = (int64_t)(int32_t)a * (int64_t)(int32_t)b;
        if (r == 900000000LL) status |= (1 << 3);
    }

    /* Bit 4: Mixed accumulation: add i16 as float, compare with integer sum */
    {
        float fsum = 0.0f;
        uint16_t isum = 0;
        for (uint8_t i = 1; i <= 10; i++) {
            fsum += (float)i;
            isum += i;
        }
        if ((uint16_t)fsum == isum && isum == 55) status |= (1 << 4);
    }

    /* Bit 5: i32 division then float: (float)(100000L / 7L) == 14285.0f */
    {
        volatile int32_t a = 100000L, b = 7;
        int32_t q = a / b;
        float f = (float)q;
        if (f == 14285.0f) status |= (1 << 5);
    }

    /* Bit 6: Float multiply then truncate to i32: (i32)(12.5f * 8.0f) == 100 */
    {
        volatile float a = 12.5f, b = 8.0f;
        int32_t r = (int32_t)(a * b);
        if (r == 100L) status |= (1 << 6);
    }

    /* Bit 7: i64 add/sub without float: 0x100000000 + 0x200000000 == 0x300000000 */
    {
        volatile uint64_t a = 0x100000000ULL, b = 0x200000000ULL;
        uint64_t sum = a + b;
        if ((uint32_t)(sum >> 32) == 3 && (uint32_t)sum == 0)
            status |= (1 << 7);
    }

    return status; /* expect 0x00FF */
}
