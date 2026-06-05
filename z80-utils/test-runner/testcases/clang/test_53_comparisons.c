/* Test 53: Comparison patterns - signed/unsigned, 8/16/32-bit, boundary values */
typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned long uint32_t;
typedef long int32_t;

int main() {
    uint16_t status = 0;

    /* Bit 0: Unsigned i16 comparisons at boundaries */
    {
        volatile uint16_t zero = 0, one = 1, max = 65535;
        if (zero < one && one < max && zero < max &&
            !(max < zero) && !(one < zero))
            status |= (1 << 0);
    }

    /* Bit 1: Signed i16 comparisons across zero */
    {
        volatile int16_t neg = -1000, zero = 0, pos = 1000;
        volatile int16_t mn = -32768, mx = 32767;
        if (neg < zero && zero < pos && mn < mx &&
            mn < neg && pos < mx)
            status |= (1 << 1);
    }

    /* Bit 2: i8 signed vs unsigned interpretation:
       (uint8_t)200 > (uint8_t)100, but (int8_t)200 == -56 < (int8_t)100 */
    {
        volatile uint8_t ua = 200, ub = 100;
        volatile int8_t sa = -56, sb = 100;
        if (ua > ub && sa < sb) status |= (1 << 2);
    }

    /* Bit 3: i32 comparisons: 100000 > 99999, -100000 < -99999 */
    {
        volatile uint32_t a = 100000UL, b = 99999UL;
        volatile int32_t c = -100000L, d = -99999L;
        if (a > b && c < d) status |= (1 << 3);
    }

    /* Bit 4: Equality tests: 0==0, 0xFFFF==0xFFFF, 0!=1 */
    {
        volatile uint16_t a = 0, b = 0, c = 0xFFFF, d = 0xFFFF, e = 1;
        if (a == b && c == d && a != e && c != e)
            status |= (1 << 4);
    }

    /* Bit 5: Chained ternary comparisons: clamp(-50, 0, 100)==0, clamp(50,0,100)==50, clamp(150,0,100)==100 */
    {
        volatile int16_t v1 = -50, v2 = 50, v3 = 150;
        int16_t lo = 0, hi = 100;
        int16_t r1 = v1 < lo ? lo : (v1 > hi ? hi : v1);
        int16_t r2 = v2 < lo ? lo : (v2 > hi ? hi : v2);
        int16_t r3 = v3 < lo ? lo : (v3 > hi ? hi : v3);
        if (r1 == 0 && r2 == 50 && r3 == 100) status |= (1 << 5);
    }

    /* Bit 6: Compare with zero (tests sign bit optimization path):
       positive > 0, negative < 0, zero == 0 */
    {
        volatile int16_t pos = 12345, neg = -12345, zero = 0;
        if (pos > 0 && neg < 0 && zero == 0 &&
            !(pos < 0) && !(neg > 0) && !(zero != 0))
            status |= (1 << 6);
    }

    /* Bit 7: Mixed comparison in conditional: abs via comparison */
    {
        volatile int16_t a = -42, b = 42, c = 0;
        int16_t abs_a = a < 0 ? -a : a;
        int16_t abs_b = b < 0 ? -b : b;
        int16_t abs_c = c < 0 ? -c : c;
        if (abs_a == 42 && abs_b == 42 && abs_c == 0) status |= (1 << 7);
    }

    return status; /* expect 0x00FF */
}
