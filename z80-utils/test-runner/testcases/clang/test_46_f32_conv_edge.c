/* Test 46: IEEE 754 conversion edge cases, mixed arithmetic stress */
/* SKIP-IF: -ffast-math */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef long int32_t;
typedef short int16_t;

typedef union { float f; uint32_t u; } f32u;

static float mk_pinf(void) { f32u x; x.u = 0x7F800000UL; return x.f; }
static float mk_ninf(void) { f32u x; x.u = 0xFF800000UL; return x.f; }
static float mk_nan(void)  { f32u x; x.u = 0x7FC00000UL; return x.f; }
static uint32_t f2u(float f) { f32u x; x.f = f; return x.u; }
static float u2f(uint32_t u) { f32u x; x.u = u; return x.f; }

float fabs_f(float x) { return x < 0.0f ? -x : x; }

int main() {
    uint16_t status = 0;

    /* === float → int32 conversion edge cases === */

    /* Bit 0: (int32_t)(+inf) clamps to INT32_MAX (0x7FFFFFFF) */
    {
        volatile float pinf = mk_pinf();
        volatile int32_t r = (int32_t)pinf;
        if (r == 2147483647L)
            status |= (1 << 0);
    }

    /* Bit 1: (int32_t)(-inf) clamps to INT32_MIN (0x80000000) */
    {
        volatile float ninf = mk_ninf();
        volatile int32_t r = (int32_t)ninf;
        if (r == -2147483648L)
            status |= (1 << 1);
    }

    /* Bit 2: (int32_t)(NaN) == 0 */
    {
        volatile float nan = mk_nan();
        volatile int32_t r = (int32_t)nan;
        if (r == 0L)
            status |= (1 << 2);
    }

    /* Bit 3: (int32_t)(0.999f) == 0, (int32_t)(-0.999f) == 0 (truncate toward zero) */
    {
        volatile float a = 0.999f, b = -0.999f;
        int32_t ra = (int32_t)a, rb = (int32_t)b;
        if (ra == 0L && rb == 0L)
            status |= (1 << 3);
    }

    /* Bit 4: (int32_t)(2147483520.0f) == 2147483520 (largest exact int < INT32_MAX) */
    {
        /* 2147483520 = 0x7FFFFF80, representable in float: 0x4EFFFFFF */
        volatile float f = u2f(0x4EFFFFFFUL);
        int32_t r = (int32_t)f;
        if (r == 2147483520L)
            status |= (1 << 4);
    }

    /* Bit 5: unsigned: (uint32_t)(-1.0f) == 0 (negative → 0 for unsigned) */
    {
        volatile float neg = -1.0f;
        volatile uint32_t r = (uint32_t)neg;
        if (r == 0UL)
            status |= (1 << 5);
    }

    /* === int32 → float conversion edge cases === */

    /* Bit 6: (float)0 == +0.0 (bit pattern 0x00000000) */
    {
        volatile int32_t z = 0;
        float f = (float)z;
        if (f2u(f) == 0x00000000UL)
            status |= (1 << 6);
    }

    /* Bit 7: (float)(-1) == -1.0f (0xBF800000) */
    {
        volatile int32_t n = -1;
        float f = (float)n;
        if (f2u(f) == 0xBF800000UL)
            status |= (1 << 7);
    }

    /* Bit 8: (float)(INT32_MIN) == -2147483648.0f (exact power of 2) */
    {
        volatile int32_t min_val = -2147483648L;
        float f = (float)min_val;
        if (f2u(f) == 0xCF000000UL) /* -2^31 */
            status |= (1 << 8);
    }

    /* Bit 9: round-trip: (int32_t)(float)(int32_t)x == x for |x| <= 2^24 */
    {
        volatile int32_t a = 16777216L; /* 2^24 */
        volatile int32_t b = -16777216L;
        volatile int32_t c = 12345678L;
        int32_t ra = (int32_t)(float)a;
        int32_t rb = (int32_t)(float)b;
        int32_t rc = (int32_t)(float)c;
        if (ra == a && rb == b && rc == c)
            status |= (1 << 9);
    }

    /* === Mixed arithmetic stress === */

    /* Bit 10: Kahan summation test - sum 0.1f ten times ≈ 1.0
       Direct sum: 0.1*10 != 1.0 due to rounding.
       With Kahan: much closer. Test that Kahan sum is closer. */
    {
        volatile float tenth = 0.1f;
        /* Direct sum */
        float direct = 0.0f;
        for (uint8_t i = 0; i < 10; i++)
            direct = direct + tenth;
        /* Kahan compensated sum */
        float kahan = 0.0f, comp = 0.0f;
        for (uint8_t i = 0; i < 10; i++) {
            float y = tenth - comp;
            float t = kahan + y;
            comp = (t - kahan) - y;
            kahan = t;
        }
        /* Kahan should be closer to 1.0 than direct */
        float d_err = fabs_f(direct - 1.0f);
        float k_err = fabs_f(kahan - 1.0f);
        if (k_err <= d_err)
            status |= (1 << 10);
    }

    /* Bit 11: Horner's method: evaluate 2x^3 - 3x^2 + x - 5 at x=3.0
       = 2*27 - 3*9 + 3 - 5 = 54 - 27 + 3 - 5 = 25.0 */
    {
        volatile float x = 3.0f;
        /* Horner: ((2*x - 3)*x + 1)*x - 5 */
        volatile float c3=2.0f, c2=-3.0f, c1=1.0f, c0=-5.0f;
        float r = ((c3 * x + c2) * x + c1) * x + c0;
        if (r == 25.0f)
            status |= (1 << 11);
    }

    /* Bit 12: reciprocal: 1/(1/x) == x for x = power of 2 */
    {
        volatile float x = 64.0f;
        float r = 1.0f / (1.0f / x);
        if (r == x)
            status |= (1 << 12);
    }

    /* Bit 13: geometric mean: sqrt(a*b) via Newton's method for a=4, b=9
       sqrt(36) == 6.0 */
    {
        volatile float a = 4.0f, b = 9.0f;
        float prod = a * b; /* 36.0 */
        /* Newton sqrt of prod */
        float guess = prod / 2.0f;
        for (uint8_t i = 0; i < 10; i++)
            guess = (guess + prod / guess) / 2.0f;
        if (fabs_f(guess - 6.0f) < 0.001f)
            status |= (1 << 13);
    }

    /* Bit 14: distributive: a*(b+c) == a*b + a*c for exact values */
    {
        volatile float a = 4.0f, b = 3.0f, c = 5.0f;
        float lhs = a * (b + c);
        float rhs = a * b + a * c;
        if (lhs == rhs && lhs == 32.0f)
            status |= (1 << 14);
    }

    /* Bit 15: repeated multiply/divide cycle: x * 7 / 7 == x for power of 2 */
    {
        volatile float x = 128.0f;
        volatile float seven = 7.0f;
        float r = x;
        for (uint8_t i = 0; i < 5; i++) {
            r = r * seven;
            r = r / seven;
        }
        if (r == x)
            status |= (1 << 15);
    }

    return status; /* expect 0xFFFF */
}
