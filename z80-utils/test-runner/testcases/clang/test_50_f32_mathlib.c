/* Test 50: f32 math library - truncf, floorf, ceilf, roundf, rintf,
   fminf, fmaxf, fmodf, lroundf */
typedef unsigned short uint16_t;

float truncf(float);
float floorf(float);
float ceilf(float);
float roundf(float);
float rintf(float);
float fminf(float, float);
float fmaxf(float, float);
float fmodf(float, float);
long lroundf(float);

int main() {
    uint16_t status = 0;

    /* Bit 0: truncf */
    {
        volatile float a = 3.7f, b = -3.7f;
        if (truncf(a) == 3.0f && truncf(b) == -3.0f)
            status |= (1 << 0);
    }

    /* Bit 1: truncf small */
    {
        volatile float a = 0.9f, b = -0.1f;
        if (truncf(a) == 0.0f && truncf(b) == 0.0f)
            status |= (1 << 1);
    }

    /* Bit 2: floorf */
    {
        volatile float a = 3.7f, b = -3.7f;
        if (floorf(a) == 3.0f && floorf(b) == -4.0f)
            status |= (1 << 2);
    }

    /* Bit 3: floorf small */
    {
        volatile float a = 0.5f, b = -0.5f;
        if (floorf(a) == 0.0f && floorf(b) == -1.0f)
            status |= (1 << 3);
    }

    /* Bit 4: ceilf */
    {
        volatile float a = 3.2f, b = -3.2f;
        if (ceilf(a) == 4.0f && ceilf(b) == -3.0f)
            status |= (1 << 4);
    }

    /* Bit 5: ceilf small */
    {
        volatile float a = 0.1f, b = -0.9f;
        if (ceilf(a) == 1.0f && ceilf(b) == 0.0f)
            status |= (1 << 5);
    }

    /* Bit 6: roundf */
    {
        volatile float a = 2.5f, b = -2.5f;
        if (roundf(a) == 3.0f && roundf(b) == -3.0f)
            status |= (1 << 6);
    }

    /* Bit 7: fminf / fmaxf (both orderings) */
    {
        volatile float a = 3.0f, b = 5.0f;
        if (fminf(a, b) == 3.0f && fmaxf(a, b) == 5.0f &&
            fminf(b, a) == 3.0f && fmaxf(b, a) == 5.0f)
            status |= (1 << 7);
    }

    /* Bit 8: fmodf */
    {
        volatile float a = 7.0f, b = 3.0f;
        if (fmodf(a, b) == 1.0f)
            status |= (1 << 8);
    }

    /* Bit 9: lroundf */
    {
        volatile float a = 3.5f, b = -2.5f;
        if (lroundf(a) == 4 && lroundf(b) == -3)
            status |= (1 << 9);
    }

    /* Bit 10: rintf (ties to even) */
    {
        volatile float a = 2.5f, b = 3.5f;
        if (rintf(a) == 2.0f && rintf(b) == 4.0f)
            status |= (1 << 10);
    }

    /* Bit 11: large values (already integer) */
    {
        volatile float a = 16777216.0f;
        if (truncf(a) == a && floorf(a) == a)
            status |= (1 << 11);
    }

    return status;
    /* expect 0x0FFF */
}
