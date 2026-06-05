/* Test 43: IEEE 754 infinity and NaN edge cases */
/* SKIP-IF: -ffast-math */
/* Uses union for bit-level float construction/inspection */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;

typedef union { float f; uint32_t u; } f32u;

/* IEEE 754 special constants */
static float mk_pinf(void)  { f32u x; x.u = 0x7F800000UL; return x.f; } /* +inf */
static float mk_ninf(void)  { f32u x; x.u = 0xFF800000UL; return x.f; } /* -inf */
static float mk_nan(void)   { f32u x; x.u = 0x7FC00000UL; return x.f; } /* qNaN */
static float mk_snan(void)  { f32u x; x.u = 0x7F800001UL; return x.f; } /* sNaN */
static float mk_fmax(void)  { f32u x; x.u = 0x7F7FFFFFUL; return x.f; } /* FLT_MAX */
static float mk_nfmax(void) { f32u x; x.u = 0xFF7FFFFFUL; return x.f; } /* -FLT_MAX */

static uint32_t f2u(float f) { f32u x; x.f = f; return x.u; }

int main() {
    uint16_t status = 0;

    /* --- Infinity identity & comparison --- */

    /* Bit 0: +inf == +inf, -inf == -inf */
    {
        volatile float pinf = mk_pinf(), pinf2 = mk_pinf();
        volatile float ninf = mk_ninf(), ninf2 = mk_ninf();
        if (pinf == pinf2 && ninf == ninf2)
            status |= (1 << 0);
    }

    /* Bit 1: +inf != -inf, +inf > FLT_MAX, -inf < -FLT_MAX */
    {
        volatile float pinf = mk_pinf(), ninf = mk_ninf();
        volatile float fmax = mk_fmax(), nfmax = mk_nfmax();
        if (pinf != ninf && pinf > fmax && ninf < nfmax)
            status |= (1 << 1);
    }

    /* --- Infinity arithmetic --- */

    /* Bit 2: +inf + 1 == +inf, +inf + FLT_MAX == +inf */
    {
        volatile float pinf = mk_pinf(), one = 1.0f, fmax = mk_fmax();
        if (pinf + one == mk_pinf() && pinf + fmax == mk_pinf())
            status |= (1 << 2);
    }

    /* Bit 3: -inf - 1 == -inf, -inf + (-FLT_MAX) == -inf */
    {
        volatile float ninf = mk_ninf(), one = 1.0f, nfmax = mk_nfmax();
        if (ninf - one == mk_ninf() && ninf + nfmax == mk_ninf())
            status |= (1 << 3);
    }

    /* Bit 4: +inf * 2 == +inf, -inf * 3 == -inf, +inf * (-1) == -inf */
    {
        volatile float pinf = mk_pinf(), ninf = mk_ninf();
        volatile float two = 2.0f, three = 3.0f, neg1 = -1.0f;
        if (pinf * two == mk_pinf() && ninf * three == mk_ninf() &&
            pinf * neg1 == mk_ninf())
            status |= (1 << 4);
    }

    /* Bit 5: +inf / 2 == +inf, 1 / +inf == 0, -1 / +inf == -0 (== 0) */
    {
        volatile float pinf = mk_pinf();
        volatile float two = 2.0f, one = 1.0f, neg1 = -1.0f;
        float r1 = pinf / two;
        float r2 = one / pinf;
        float r3 = neg1 / pinf;
        if (r1 == mk_pinf() && r2 == 0.0f && r3 == 0.0f)
            status |= (1 << 5);
    }

    /* --- NaN behavior --- */

    /* Bit 6: NaN != NaN, !(NaN == NaN) */
    {
        volatile float nan1 = mk_nan(), nan2 = mk_nan();
        if (nan1 != nan2 && !(nan1 == nan2))
            status |= (1 << 6);
    }

    /* Bit 7: NaN is unordered: !(NaN < 1), !(NaN > 1), !(NaN <= 1), !(NaN >= 1) */
    {
        volatile float nan = mk_nan(), one = 1.0f;
        if (!(nan < one) && !(nan > one) && !(nan <= one) && !(nan >= one))
            status |= (1 << 7);
    }

    /* Bit 8: NaN + 1 == NaN (bit pattern check: exp=0xFF, mantissa!=0) */
    {
        volatile float nan = mk_nan(), one = 1.0f;
        uint32_t r = f2u(nan + one);
        if ((r & 0x7F800000UL) == 0x7F800000UL && (r & 0x007FFFFFUL) != 0)
            status |= (1 << 8);
    }

    /* Bit 9: NaN * 5 == NaN, NaN / 3 == NaN */
    {
        volatile float nan = mk_nan(), five = 5.0f, three = 3.0f;
        uint32_t r1 = f2u(nan * five);
        uint32_t r2 = f2u(nan / three);
        if ((r1 & 0x7F800000UL) == 0x7F800000UL && (r1 & 0x007FFFFFUL) != 0 &&
            (r2 & 0x7F800000UL) == 0x7F800000UL && (r2 & 0x007FFFFFUL) != 0)
            status |= (1 << 9);
    }

    /* --- Infinity-generating operations --- */

    /* Bit 10: FLT_MAX + FLT_MAX == +inf */
    {
        volatile float fmax = mk_fmax();
        uint32_t r = f2u(fmax + fmax);
        if (r == 0x7F800000UL) /* +inf */
            status |= (1 << 10);
    }

    /* Bit 11: FLT_MAX * 2 == +inf, (-FLT_MAX) * 2 == -inf */
    {
        volatile float fmax = mk_fmax(), nfmax = mk_nfmax(), two = 2.0f;
        uint32_t r1 = f2u(fmax * two);
        uint32_t r2 = f2u(nfmax * two);
        if (r1 == 0x7F800000UL && r2 == 0xFF800000UL)
            status |= (1 << 11);
    }

    /* --- NaN-generating operations --- */

    /* Bit 12: +inf + (-inf) == NaN */
    {
        volatile float pinf = mk_pinf(), ninf = mk_ninf();
        uint32_t r = f2u(pinf + ninf);
        if ((r & 0x7F800000UL) == 0x7F800000UL && (r & 0x007FFFFFUL) != 0)
            status |= (1 << 12);
    }

    /* Bit 13: 0 * inf == NaN */
    {
        volatile float zero = 0.0f, pinf = mk_pinf();
        uint32_t r = f2u(zero * pinf);
        if ((r & 0x7F800000UL) == 0x7F800000UL && (r & 0x007FFFFFUL) != 0)
            status |= (1 << 13);
    }

    /* Bit 14: 0 / 0 == NaN */
    {
        volatile float z1 = 0.0f, z2 = 0.0f;
        uint32_t r = f2u(z1 / z2);
        if ((r & 0x7F800000UL) == 0x7F800000UL && (r & 0x007FFFFFUL) != 0)
            status |= (1 << 14);
    }

    /* Bit 15: inf / inf == NaN */
    {
        volatile float p1 = mk_pinf(), p2 = mk_pinf();
        uint32_t r = f2u(p1 / p2);
        if ((r & 0x7F800000UL) == 0x7F800000UL && (r & 0x007FFFFFUL) != 0)
            status |= (1 << 15);
    }

    return status; /* expect 0xFFFF */
}
