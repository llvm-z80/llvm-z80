/* Test 60: float -> __int128 conversion (__fixsfti runtime) */
/* Exercises the f32 -> i128 conversion libcall: truncation toward zero,
   negative values, exact wide powers of two, and mantissa placement at
   large exponents. Sources are volatile so the conversions cannot be
   constant-folded away. */
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef __int128 ti;
typedef union { float f; uint32_t u; } f32u;

static float mk(uint32_t u) { f32u x; x.u = u; return x.f; }

int main(void) {
    uint16_t status = 0;

    /* Bit 0: 0.0 -> 0 */
    { volatile float a = 0.0f; if ((ti)a == 0) status |= (1 << 0); }
    /* Bit 1: 1.5 truncates to 1 */
    { volatile float a = 1.5f; if ((ti)a == 1) status |= (1 << 1); }
    /* Bit 2: -1.5 truncates to -1 */
    { volatile float a = -1.5f; if ((ti)a == -1) status |= (1 << 2); }
    /* Bit 3: fractional part dropped */
    { volatile float a = 123456.875f; if ((ti)a == 123456) status |= (1 << 3); }
    /* Bit 4: exact negative power of two */
    { volatile float a = -8388608.0f; if ((ti)a == -8388608L) status |= (1 << 4); }
    /* Bit 5: below one truncates to 0 */
    { volatile float a = 0.99f; if ((ti)a == 0) status |= (1 << 5); }
    /* Bit 6: 2^100 */
    { volatile float a = mk(0x71800000UL); if ((ti)a == ((ti)1 << 100)) status |= (1 << 6); }
    /* Bit 7: -2^100 */
    { volatile float a = mk(0xF1800000UL); if ((ti)a == -((ti)1 << 100)) status |= (1 << 7); }
    /* Bit 8: full mantissa at a large exponent: 0xFFFFFF * 2^80 */
    { volatile float a = mk(0x737FFFFFUL); if ((ti)a == ((ti)0xFFFFFFUL << 80)) status |= (1 << 8); }
    /* Bit 9: 2^126 */
    { volatile float a = mk(0x7E800000UL); if ((ti)a == ((ti)1 << 126)) status |= (1 << 9); }
    /* Bit 10: -2^126 */
    { volatile float a = mk(0xFE800000UL); if ((ti)a == -((ti)1 << 126)) status |= (1 << 10); }
    /* Bit 11: small positive truncation */
    { volatile float a = 3.75f; if ((ti)a == 3) status |= (1 << 11); }
    /* Bit 12: negative fraction truncates to 0 */
    { volatile float a = -0.5f; if ((ti)a == 0) status |= (1 << 12); }
    /* Bit 13: odd 24-bit mantissa kept exactly */
    { volatile float a = 8388609.0f; if ((ti)a == 8388609L) status |= (1 << 13); }
    /* Bit 14: 2^32 crosses the 4-byte boundary */
    { volatile float a = 4294967296.0f; if ((ti)a == ((ti)1 << 32)) status |= (1 << 14); }
    /* Bit 15: mixed bits across the boundary, negated */
    { volatile float a = -4294967808.0f;
      if ((ti)a == -(((ti)1 << 32) + 512)) status |= (1 << 15); }

    return status; /* expect 0xFFFF */
}
