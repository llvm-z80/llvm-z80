// RUN: %cc %s -o %t
// RUN: %run --expect-result=0xFFFF %t

/* modff runtime routine: integral and fractional parts, both with the sign
   of the input. Signed zeros are compared as bit patterns. */
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef union { float f; uint32_t u; } f32u;

static uint32_t bits(float f) { f32u x; x.f = f; return x.u; }
static float mk(uint32_t u) { f32u x; x.u = u; return x.f; }

int main(void) {
    uint16_t status = 0;
    float ip;

    { volatile float a = 3.75f; float fr = __builtin_modff(a, &ip);
      if (ip == 3.0f && fr == 0.75f) status |= (1 << 0); }
    { volatile float a = -3.75f; float fr = __builtin_modff(a, &ip);
      if (ip == -3.0f && fr == -0.75f) status |= (1 << 1); }
    { volatile float a = 0.5f; float fr = __builtin_modff(a, &ip);
      if (ip == 0.0f && fr == 0.5f) status |= (1 << 2); }
    { volatile float a = -0.5f; float fr = __builtin_modff(a, &ip);
      if (bits(ip) == 0x80000000UL && fr == -0.5f) status |= (1 << 3); }
    { volatile float a = 123456.875f; float fr = __builtin_modff(a, &ip);
      if (ip == 123456.0f && fr == 0.875f) status |= (1 << 4); }
    { volatile float a = 0.0f; float fr = __builtin_modff(a, &ip);
      if (bits(ip) == 0UL && bits(fr) == 0UL) status |= (1 << 5); }
    { volatile float a = mk(0x80000000UL); float fr = __builtin_modff(a, &ip);
      if (bits(ip) == 0x80000000UL && bits(fr) == 0x80000000UL) status |= (1 << 6); }
    { volatile float a = 8388609.0f; float fr = __builtin_modff(a, &ip);
      if (ip == 8388609.0f && bits(fr) == 0UL) status |= (1 << 7); }
    { volatile float a = mk(0x7F800000UL); float fr = __builtin_modff(a, &ip);
      if (bits(ip) == 0x7F800000UL && bits(fr) == 0UL) status |= (1 << 8); }
    { volatile float a = mk(0xFF800000UL); float fr = __builtin_modff(a, &ip);
      if (bits(ip) == 0xFF800000UL && bits(fr) == 0x80000000UL) status |= (1 << 9); }
    /* A negative integral value: the fraction is -0.0, with the sign of the
       input. A subtraction would give +0.0. */
    { volatile float a = -3.0f; float fr = __builtin_modff(a, &ip);
      if (bits(ip) == 0xC0400000UL && bits(fr) == 0x80000000UL) status |= (1 << 10); }
    /* Just under 1.0 */
    { volatile float a = 0.99999994f; float fr = __builtin_modff(a, &ip);
      if (bits(ip) == 0UL && fr == a) status |= (1 << 11); }
    /* A NaN gives NaN in both halves */
    { volatile float a = mk(0x7FC00001UL); float fr = __builtin_modff(a, &ip);
      if ((bits(ip) & 0x7F800000UL) == 0x7F800000UL && (bits(ip) & 0x7FFFFFUL) != 0UL
          && (bits(fr) & 0x7F800000UL) == 0x7F800000UL && (bits(fr) & 0x7FFFFFUL) != 0UL)
        status |= (1 << 12); }
    /* A denormal is entirely fractional */
    { volatile float a = mk(0x00000001UL); float fr = __builtin_modff(a, &ip);
      if (bits(ip) == 0UL && bits(fr) == 0x00000001UL) status |= (1 << 13); }
    /* Halfway values keep full precision on both sides */
    { volatile float a = 2.5f; float fr = __builtin_modff(a, &ip);
      if (ip == 2.0f && fr == 0.5f) status |= (1 << 14); }
    { volatile float a = -8388607.5f; float fr = __builtin_modff(a, &ip);
      if (ip == -8388607.0f && fr == -0.5f) status |= (1 << 15); }

    return status;
}
