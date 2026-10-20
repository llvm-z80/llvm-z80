// RUN: %cc %s -o %t
// RUN: %run --expect-result=0xFFFF %t
// UNSUPPORTED: fast-math

/* IEEE 754 rounding, precision limits and catastrophic cancellation in f32. */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef long int32_t;

typedef union { float f; uint32_t u; } f32u;

static uint32_t f2u(float f) { f32u x; x.f = f; return x.u; }
static float u2f(uint32_t u) { f32u x; x.u = u; return x.f; }

int main() {
    uint16_t status = 0;

    /* === Round-to-nearest-even (banker's rounding) === */

    /* Bit 0: 1.0 + 2^-24 is a tie and rounds to even, so stays 1.0 */
    {
        volatile float one = 1.0f;
        volatile float eps = u2f(0x33800000UL); /* 2^-24 = 5.96e-8 */
        float r = one + eps;
        if (f2u(r) == 0x3F800000UL) /* stays 1.0 */
            status |= (1 << 0);
    }

    /* Bit 1: 1.0 + 1.5*2^-24 is past the midpoint and rounds up */
    {
        volatile float one = 1.0f;
        volatile float eps = u2f(0x33C00000UL); /* 1.5 * 2^-24 */
        float r = one + eps;
        if (f2u(r) == 0x3F800001UL) /* 1.0 + 2^-23 */
            status |= (1 << 1);
    }

    /* Bit 2: (1 + 2^-23)^2 = 1 + 2^-22 + 2^-46 rounds to 1 + 2^-22 */
    {
        volatile float a = u2f(0x3F800001UL); /* 1 + 2^-23 */
        float r = a * a;
        if (f2u(r) == 0x3F800002UL) /* 1 + 2^-22 */
            status |= (1 << 2);
    }

    /* Bit 3: 1.0 / 3.0 rounds up to 0x3EAAAAAB */
    {
        volatile float one = 1.0f, three = 3.0f;
        float r = one / three;
        if (f2u(r) == 0x3EAAAAABL)
            status |= (1 << 3);
    }

    /* === Precision limits === */

    /* Bit 4: 2^24 + 1 rounds back to 2^24 */
    {
        volatile float big = 16777216.0f;  /* 2^24 */
        volatile float one = 1.0f;
        float r = big + one;
        if (r == big) /* can't represent 2^24+1 */
            status |= (1 << 4);
    }

    /* Bit 5: 2^24 + 2 is exact */
    {
        volatile float big = 16777216.0f;
        volatile float two = 2.0f;
        float r = big + two;
        if (f2u(r) == 0x4B800001UL) /* 16777218 */
            status |= (1 << 5);
    }

    /* Bit 6: 2^23 - 1 + 0.5 == 8388607.5 exactly */
    {
        volatile float a = 8388607.0f;
        volatile float h = 0.5f;
        float r = a + h;
        if (r == 8388607.5f)
            status |= (1 << 6);
    }

    /* Bit 7: (float)2147483647 rounds up to 2^31 */
    {
        volatile int32_t big = 2147483647L;
        volatile float f = (float)big;
        if (f2u(f) == 0x4F000000UL) /* 2^31 = 2147483648.0 */
            status |= (1 << 7);
    }

    /* === Catastrophic cancellation === */

    /* Bit 8: 1e10 + 1.0 - 1e10 != 1.0, the 1.0 is lost */
    {
        volatile float big = 1e10f, one = 1.0f;
        float r = (big + one) - big;
        if (r != one)
            status |= (1 << 8);
    }

    /* Bit 9: nearly equal subtraction: nextafter(1.0) - 1.0 = 2^-23 */
    {
        volatile float a = u2f(0x3F800001UL); /* 1 + 2^-23 */
        volatile float b = 1.0f;
        float diff = a - b;
        if (f2u(diff) == 0x34000000UL) /* 2^-23 = 1.19209e-7 */
            status |= (1 << 9);
    }

    /* === Chained operations === */

    /* Bit 10: (3.0 * 5.0 + 4.0 * 7.0) / 43.0 == 1.0 */
    {
        volatile float a=3.0f, b=5.0f, c=4.0f, d=7.0f, e=43.0f;
        float r = (a * b + c * d) / e;
        if (r == 1.0f)
            status |= (1 << 10);
    }

    /* Bit 11: halving 1.0 23 times gives exactly 2^-23 */
    {
        volatile float x = 1.0f;
        volatile float two = 2.0f;
        uint8_t ok = 1;
        for (uint8_t i = 0; i < 23; i++) {
            x = x / two;
        }
        if (f2u(x) == 0x34000000UL)
            status |= (1 << 11);
    }

    /* Bit 12: sum of 1/2^i for i=1..23 is 1 - 2^-23 = 0x3F7FFFFE */
    {
        volatile float sum = 0.0f, term = 0.5f;
        volatile float two = 2.0f;
        for (uint8_t i = 0; i < 23; i++) {
            sum = sum + term;
            term = term / two;
        }
        if (f2u(sum) == 0x3F7FFFFEUL) /* 1.0 - 2^-23 */
            status |= (1 << 12);
    }

    /* === Sign preservation through operations === */

    /* Bit 13: (-a) * (-b) == a * b exactly (sign cancellation) */
    {
        volatile float a = 3.14f, b = 2.71f;
        volatile float na = -3.14f, nb = -2.71f;
        if (f2u(a * b) == f2u(na * nb))
            status |= (1 << 13);
    }

    /* Bit 14: (-a) / (-b) == a / b exactly */
    {
        volatile float a = 7.5f, b = 2.5f;
        volatile float na = -7.5f, nb = -2.5f;
        if (f2u(a / b) == f2u(na / nb))
            status |= (1 << 14);
    }

    /* Bit 15: a * 1.0 == a exactly for three values */
    {
        volatile float one = 1.0f;
        volatile float a = 3.14159f, b = -273.15f, c = 0.001f;
        if (f2u(a * one) == f2u(a) &&
            f2u(b * one) == f2u(b) &&
            f2u(c * one) == f2u(c))
            status |= (1 << 15);
    }

    return status;
}
