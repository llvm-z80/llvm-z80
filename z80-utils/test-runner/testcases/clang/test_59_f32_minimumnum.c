/* Test 59: IEEE 754-2019 minimumNumber / maximumNumber (f32) */
/* Exercises G_FMINIMUMNUM/G_FMAXIMUMNUM legalization and the
   fminimum_numf / fmaximum_numf runtime, including precise signed-zero order
   (-0 < +0). Under -ffast-math these collapse to fminf/fmaxf, where the
   signed-zero result is unspecified, so skip there. */
/* SKIP-IF: -ffast-math */
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;

typedef union { float f; uint32_t u; } f32u;

static float mk(uint32_t u) { f32u x; x.u = u; return x.f; }
static uint32_t f2u(float f) { f32u x; x.f = f; return x.u; }

#define PZERO 0x00000000UL /* +0.0 */
#define NZERO 0x80000000UL /* -0.0 */
#define QNAN  0x7FC00000UL /* quiet NaN */

int main() {
    uint16_t status = 0;

    /* === Ordinary selection === */

    /* Bit 0: minimumNumber(2, 3) == 2 */
    { volatile float a = 2.0f, b = 3.0f;
      if (__builtin_fminimum_numf(a, b) == 2.0f) status |= (1 << 0); }

    /* Bit 1: minimumNumber(3, 2) == 2 */
    { volatile float a = 3.0f, b = 2.0f;
      if (__builtin_fminimum_numf(a, b) == 2.0f) status |= (1 << 1); }

    /* Bit 2: maximumNumber(2, 3) == 3 */
    { volatile float a = 2.0f, b = 3.0f;
      if (__builtin_fmaximum_numf(a, b) == 3.0f) status |= (1 << 2); }

    /* Bit 3: maximumNumber(3, 2) == 3 */
    { volatile float a = 3.0f, b = 2.0f;
      if (__builtin_fmaximum_numf(a, b) == 3.0f) status |= (1 << 3); }

    /* Bit 4: minimumNumber(-3, 3) == -3 */
    { volatile float a = -3.0f, b = 3.0f;
      if (__builtin_fminimum_numf(a, b) == -3.0f) status |= (1 << 4); }

    /* Bit 5: maximumNumber(-3, 3) == 3 */
    { volatile float a = -3.0f, b = 3.0f;
      if (__builtin_fmaximum_numf(a, b) == 3.0f) status |= (1 << 5); }

    /* === NaN handling: return the non-NaN operand === */

    /* Bit 6: minimumNumber(NaN, 5) == 5 */
    { volatile float a = mk(QNAN), b = 5.0f;
      if (__builtin_fminimum_numf(a, b) == 5.0f) status |= (1 << 6); }

    /* Bit 7: minimumNumber(5, NaN) == 5 */
    { volatile float a = 5.0f, b = mk(QNAN);
      if (__builtin_fminimum_numf(a, b) == 5.0f) status |= (1 << 7); }

    /* Bit 8: maximumNumber(NaN, 5) == 5 */
    { volatile float a = mk(QNAN), b = 5.0f;
      if (__builtin_fmaximum_numf(a, b) == 5.0f) status |= (1 << 8); }

    /* === Signed zero order: -0 < +0 (the precise behavior) === */

    /* Bit 9: minimumNumber(-0, +0) == -0 */
    { volatile float a = mk(NZERO), b = mk(PZERO);
      if (f2u(__builtin_fminimum_numf(a, b)) == NZERO) status |= (1 << 9); }

    /* Bit 10: minimumNumber(+0, -0) == -0 */
    { volatile float a = mk(PZERO), b = mk(NZERO);
      if (f2u(__builtin_fminimum_numf(a, b)) == NZERO) status |= (1 << 10); }

    /* Bit 11: maximumNumber(-0, +0) == +0 */
    { volatile float a = mk(NZERO), b = mk(PZERO);
      if (f2u(__builtin_fmaximum_numf(a, b)) == PZERO) status |= (1 << 11); }

    /* Bit 12: maximumNumber(+0, -0) == +0 */
    { volatile float a = mk(PZERO), b = mk(NZERO);
      if (f2u(__builtin_fmaximum_numf(a, b)) == PZERO) status |= (1 << 12); }

    /* Bit 13: minimumNumber(-0, -0) == -0 */
    { volatile float a = mk(NZERO), b = mk(NZERO);
      if (f2u(__builtin_fminimum_numf(a, b)) == NZERO) status |= (1 << 13); }

    /* Bit 14: maximumNumber(+0, +0) == +0 */
    { volatile float a = mk(PZERO), b = mk(PZERO);
      if (f2u(__builtin_fmaximum_numf(a, b)) == PZERO) status |= (1 << 14); }

    /* Bit 15: minimumNumber(1.5, -2.5) == -2.5 */
    { volatile float a = 1.5f, b = -2.5f;
      if (__builtin_fminimum_numf(a, b) == -2.5f) status |= (1 << 15); }

    return status; /* expect 0xFFFF */
}
