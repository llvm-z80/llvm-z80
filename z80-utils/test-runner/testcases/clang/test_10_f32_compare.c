/* Test 10: f32 compare - ordered comparisons, fabs, sqrt, lerp */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

float fabs_f(float x) {
    return x < 0.0f ? -x : x;
}

/* Newton's method sqrt approximation */
float sqrt_f(float x) {
    float guess = x / 2.0f;
    for (uint8_t i = 0; i < 10; i++) {
        guess = (guess + x / guess) / 2.0f;
    }
    return guess;
}

/* Linear interpolation */
float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

int main() {
    uint16_t status = 0;

    /* Bit 0: ordered comparisons:
       1.0f < 2.0f, 2.0f > 1.0f, 1.0f <= 1.0f, 1.0f >= 1.0f,
       1.0f == 1.0f, 1.0f != 2.0f */
    {
        volatile float one = 1.0f, two = 2.0f, one2 = 1.0f;
        if (one < two && two > one && one <= one2 &&
            one >= one2 && one == one2 && one != two)
            status |= (1 << 0);
    }

    /* Bit 1: fabs: fabs_f(-3.5f)==3.5f, fabs_f(3.5f)==3.5f, fabs_f(0.0f)==0.0f */
    {
        volatile float a = -3.5f, b = 3.5f, c = 0.0f;
        if (fabs_f(a) == 3.5f && fabs_f(b) == 3.5f && fabs_f(c) == 0.0f)
            status |= (1 << 1);
    }

    /* Bit 2: Newton's sqrt: sqrt_f(4.0f) close to 2.0f (within 0.01);
       sqrt_f(9.0f) close to 3.0f */
    {
        float r1 = sqrt_f(4.0f);
        float r2 = sqrt_f(9.0f);
        if (fabs_f(r1 - 2.0f) < 0.01f && fabs_f(r2 - 3.0f) < 0.01f)
            status |= (1 << 2);
    }

    /* Bit 3: lerp: lerp(0,10,0.5)==5; lerp(0,10,0)==0; lerp(0,10,1)==10 */
    {
        float r1 = lerp(0.0f, 10.0f, 0.5f);
        float r2 = lerp(0.0f, 10.0f, 0.0f);
        float r3 = lerp(0.0f, 10.0f, 1.0f);
        if (fabs_f(r1 - 5.0f) < 0.01f &&
            fabs_f(r2 - 0.0f) < 0.01f &&
            fabs_f(r3 - 10.0f) < 0.01f)
            status |= (1 << 3);
    }

    /* Bit 4: Negative comparisons: -5.0 < -1.0, -1.0 < 0.0 */
    {
        volatile float a = -5.0f, b = -1.0f, c = 0.0f;
        if (a < b && b < c && a < c) status |= (1 << 4);
    }

    /* Bit 5: sqrt_f(100.0) close to 10.0, sqrt_f(1.0)==1.0 */
    {
        float r1 = sqrt_f(100.0f);
        float r2 = sqrt_f(1.0f);
        if (fabs_f(r1 - 10.0f) < 0.01f && fabs_f(r2 - 1.0f) < 0.01f)
            status |= (1 << 5);
    }

    /* Bit 6: lerp negative range: lerp(-10, 10, 0.25) == -5.0 */
    {
        float r = lerp(-10.0f, 10.0f, 0.25f);
        if (fabs_f(r - (-5.0f)) < 0.01f) status |= (1 << 6);
    }

    /* Bit 7: Max/min via comparison: max(3.5, 7.2)==7.2, min(3.5, 7.2)==3.5 */
    {
        volatile float a = 3.5f, b = 7.2f;
        float mx = a > b ? a : b;
        float mn = a < b ? a : b;
        if (fabs_f(mx - 7.2f) < 0.01f && fabs_f(mn - 3.5f) < 0.01f)
            status |= (1 << 7);
    }

    return status; /* expect 0x00FF */
}
