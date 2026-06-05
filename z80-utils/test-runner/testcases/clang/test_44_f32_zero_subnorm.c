/* Test 44: IEEE 754 signed zero, subnormals, and boundary values */
/* SKIP-IF: -ffast-math */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;

typedef union { float f; uint32_t u; } f32u;

static float mk_pzero(void)  { f32u x; x.u = 0x00000000UL; return x.f; } /* +0.0 */
static float mk_nzero(void)  { f32u x; x.u = 0x80000000UL; return x.f; } /* -0.0 */
static float mk_min_subn(void) { f32u x; x.u = 0x00000001UL; return x.f; } /* smallest subnormal ~1.4e-45 */
static float mk_max_subn(void) { f32u x; x.u = 0x007FFFFFUL; return x.f; } /* largest subnormal ~1.175e-38 */
static float mk_min_norm(void) { f32u x; x.u = 0x00800000UL; return x.f; } /* smallest normal = FLT_MIN */
static float mk_fmax(void)    { f32u x; x.u = 0x7F7FFFFFUL; return x.f; } /* FLT_MAX */

static uint32_t f2u(float f) { f32u x; x.f = f; return x.u; }

int main() {
    uint16_t status = 0;

    /* === Signed zero === */

    /* Bit 0: +0 == -0 (IEEE 754 mandates equality) */
    {
        volatile float pz = mk_pzero(), nz = mk_nzero();
        if (pz == nz && !(pz != nz) && !(pz < nz) && !(pz > nz))
            status |= (1 << 0);
    }

    /* Bit 1: +0 and -0 have different bit patterns */
    {
        volatile float pz = mk_pzero(), nz = mk_nzero();
        if (f2u(pz) == 0x00000000UL && f2u(nz) == 0x80000000UL)
            status |= (1 << 1);
    }

    /* Bit 2: 1.0 + (-1.0) == +0 (sign of zero from addition) */
    {
        volatile float one = 1.0f, neg1 = -1.0f;
        float r = one + neg1;
        if (r == 0.0f && f2u(r) == 0x00000000UL) /* result is +0 */
            status |= (1 << 2);
    }

    /* Bit 3: (-1.0) + 1.0 == +0, (-0.0) + 0.0 == +0 */
    {
        volatile float neg1 = -1.0f, one = 1.0f;
        volatile float nz = mk_nzero(), pz = mk_pzero();
        float r1 = neg1 + one;
        float r2 = nz + pz;
        if (f2u(r1) == 0x00000000UL && f2u(r2) == 0x00000000UL)
            status |= (1 << 3);
    }

    /* === Subnormal numbers === */

    /* Bit 4: subnormals compare correctly: 0 < min_subn < max_subn < min_norm */
    {
        volatile float z = 0.0f;
        volatile float ms = mk_min_subn(), xs = mk_max_subn(), mn = mk_min_norm();
        if (z < ms && ms < xs && xs < mn)
            status |= (1 << 4);
    }

    /* Bit 5: subnormal + subnormal: max_subn + min_subn > max_subn */
    {
        volatile float ms = mk_min_subn(), xs = mk_max_subn();
        float r = xs + ms;
        if (r > xs && f2u(r) == 0x00800000UL) /* should become min_norm */
            status |= (1 << 5);
    }

    /* Bit 6: min_norm - min_subn == max_subn (normal → subnormal transition) */
    {
        volatile float mn = mk_min_norm(), ms = mk_min_subn();
        float r = mn - ms;
        if (f2u(r) == 0x007FFFFFUL) /* max_subn */
            status |= (1 << 6);
    }

    /* Bit 7: subnormal * 2 may become normal or stay subnormal */
    {
        volatile float xs = mk_max_subn(), two = 2.0f;
        float r = xs * two;
        /* max_subn * 2 = 2 * (2^-126 - 2^-149) = 2^-125 - 2^-148
           This is a normal number: 0x00FFFFFE (approx) */
        if (r > mk_min_norm())
            status |= (1 << 7);
    }

    /* === Boundary values === */

    /* Bit 8: FLT_MAX - FLT_MAX == 0 */
    {
        volatile float fmax = mk_fmax();
        float r = fmax - fmax;
        if (r == 0.0f)
            status |= (1 << 8);
    }

    /* Bit 9: FLT_MAX / FLT_MAX == 1.0 */
    {
        volatile float fmax = mk_fmax();
        float r = fmax / fmax;
        if (r == 1.0f)
            status |= (1 << 9);
    }

    /* Bit 10: min_norm / 2 becomes subnormal (0x00400000) */
    {
        volatile float mn = mk_min_norm(), two = 2.0f;
        float r = mn / two;
        if (f2u(r) == 0x00400000UL) /* half of min_norm is subnormal */
            status |= (1 << 10);
    }

    /* Bit 11: 1.0 / FLT_MAX is very small but not zero */
    {
        volatile float one = 1.0f, fmax = mk_fmax();
        float r = one / fmax;
        if (r > 0.0f && r < mk_min_norm())
            status |= (1 << 11);
    }

    /* === Exact arithmetic on powers of 2 === */

    /* Bit 12: 0.5 + 0.5 == 1.0 exactly, 0.25 + 0.75 == 1.0 exactly */
    {
        volatile float h = 0.5f, q = 0.25f, tq = 0.75f;
        if (h + h == 1.0f && q + tq == 1.0f)
            status |= (1 << 12);
    }

    /* Bit 13: 2^23 (8388608) is exact in float, 2^24 (16777216) is exact too */
    {
        volatile float a = 8388608.0f, b = 16777216.0f;
        volatile float one = 1.0f;
        if (a + one == 8388609.0f && b + one == b) /* 2^24+1 rounds to 2^24! */
            status |= (1 << 13);
    }

    /* Bit 14: negative subnormal: -(min_subn) is valid */
    {
        volatile float ms = mk_min_subn();
        float neg = -ms;
        if (neg < 0.0f && f2u(neg) == 0x80000001UL)
            status |= (1 << 14);
    }

    /* Bit 15: FLT_MAX + 1.0 == FLT_MAX (1 is too small to affect FLT_MAX) */
    {
        volatile float fmax = mk_fmax(), one = 1.0f;
        float r = fmax + one;
        if (r == fmax)
            status |= (1 << 15);
    }

    return status; /* expect 0xFFFF */
}
