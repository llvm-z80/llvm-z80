/* Test 02: i16 signed arithmetic - multiply, division, abs, sign ext, comparisons */
typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned long uint32_t;
typedef long int32_t;

int16_t abs16(int16_t x) {
    return x < 0 ? -x : x;
}

int main() {
    uint16_t status = 0;

    /* Bit 0: Signed multiply: (-7)*3==-21, 6*(-5)==-30 */
    {
        volatile int16_t a = -7, b = 3;
        volatile int16_t c = 6, d = -5;
        int16_t r1 = a * b;
        int16_t r2 = c * d;
        if (r1 == -21 && r2 == -30) status |= (1 << 0);
    }

    /* Bit 1: Signed division all combos:
       -17/5==-3, 17/(-5)==-3, (-17)/(-5)==3, -17%5==-2 */
    {
        volatile int16_t a = -17, b = 5;
        volatile int16_t c = 17, d = -5;
        volatile int16_t e = -17, f = -5;
        int16_t q1 = a / b;   /* -3 (truncation toward zero) */
        int16_t q2 = c / d;   /* -3 */
        int16_t q3 = e / f;   /* 3 */
        int16_t m1 = a % b;   /* -2 */
        if (q1 == -3 && q2 == -3 && q3 == 3 && m1 == -2)
            status |= (1 << 1);
    }

    /* Bit 2: safe_abs16: abs(42)==42, abs(-42)==42, abs(0)==0, abs(-100)==100 */
    {
        volatile int16_t v1 = 42, v2 = -42, v3 = 0, v4 = -100;
        if (abs16(v1) == 42 && abs16(v2) == 42 &&
            abs16(v3) == 0 && abs16(v4) == 100)
            status |= (1 << 2);
    }

    /* Bit 3: Sign extension chain:
       (int8_t)-5 -> (int16_t) should be -5 (0xFFFB);
       (int32_t)-5 from i16 -> check low16==0xFFFB and high16==0xFFFF */
    {
        volatile int8_t narrow = -5;
        volatile int16_t mid = (int16_t)narrow;
        volatile int32_t wide = (int32_t)mid;
        uint16_t low16 = (uint16_t)(wide & 0xFFFF);
        uint16_t high16 = (uint16_t)((wide >> 16) & 0xFFFF);
        if (mid == -5 && low16 == 0xFFFB && high16 == 0xFFFF)
            status |= (1 << 3);
    }

    /* Bit 4: Signed comparison chain: -100 < -1 < 0 < 1 < 100 */
    {
        volatile int16_t a = -100, b = -1, c = 0, d = 1, e = 100;
        if (a < b && b < c && c < d && d < e)
            status |= (1 << 4);
    }

    /* Bit 5: Negative multiply then divide: (-300)*(-200)/100 == 600 */
    {
        volatile int16_t a = -300, b = -200;
        int32_t product = (int32_t)a * (int32_t)b;
        int32_t result = product / 100;
        if (result == 600) status |= (1 << 5);
    }

    /* Bit 6: Signed min/max: min(-32768, 0)==-32768, max(32767, 0)==32767 */
    {
        volatile int16_t a = -32768, b = 0, c = 32767;
        int16_t mn = a < b ? a : b;
        int16_t mx = c > b ? c : b;
        if (mn == -32768 && mx == 32767) status |= (1 << 6);
    }

    /* Bit 7: Signed modulo: -17%5==-2, 17%(-5)==2 (C99 truncation toward zero) */
    {
        volatile int16_t a = -17, b = 5, c = 17, d = -5;
        int16_t r1 = a % b;
        int16_t r2 = c % d;
        if (r1 == -2 && r2 == 2) status |= (1 << 7);
    }

    return status; /* expect 0x00FF */
}
