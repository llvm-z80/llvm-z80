/* Test 08: f32 arithmetic - add, sub, mul, div, accumulation, negatives */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

int main() {
    uint16_t status = 0;

    /* Bit 0: 1.5f + 2.25f == 3.75f; 10.0f - 3.5f == 6.5f */
    {
        volatile float a = 1.5f, b = 2.25f;
        volatile float c = 10.0f, d = 3.5f;
        float sum = a + b;
        float diff = c - d;
        if (sum == 3.75f && diff == 6.5f) status |= (1 << 0);
    }

    /* Bit 1: 3.0f * 4.0f == 12.0f; 15.0f / 3.0f == 5.0f */
    {
        volatile float a = 3.0f, b = 4.0f;
        volatile float c = 15.0f, d = 3.0f;
        float prod = a * b;
        float quot = c / d;
        if (prod == 12.0f && quot == 5.0f) status |= (1 << 1);
    }

    /* Bit 2: accumulation: r=1.0; r*=2 five times -> r==32.0f */
    {
        volatile float r = 1.0f;
        volatile float two = 2.0f;
        for (uint8_t i = 0; i < 5; i++)
            r = r * two;
        if (r == 32.0f) status |= (1 << 2);
    }

    /* Bit 3: negative: (-3.0f)*(-4.0f)==12.0f; (-10.0f)+3.0f==-7.0f */
    {
        volatile float a = -3.0f, b = -4.0f;
        volatile float c = -10.0f, d = 3.0f;
        float prod = a * b;
        float sum = c + d;
        if (prod == 12.0f && sum == -7.0f) status |= (1 << 3);
    }

    /* Bit 4: Mixed add/mul: (2.5 + 3.5) * 2.0 == 12.0 */
    {
        volatile float a = 2.5f, b = 3.5f, c = 2.0f;
        float r = (a + b) * c;
        if (r == 12.0f) status |= (1 << 4);
    }

    /* Bit 5: Division by powers of 2: 128.0 / 4.0 / 2.0 == 16.0 */
    {
        volatile float a = 128.0f;
        float r = a / 4.0f / 2.0f;
        if (r == 16.0f) status |= (1 << 5);
    }

    /* Bit 6: Sum loop: 0.25 * 8 iterations == 2.0 */
    {
        volatile float sum = 0.0f;
        volatile float inc = 0.25f;
        for (uint8_t i = 0; i < 8; i++) sum = sum + inc;
        if (sum == 2.0f) status |= (1 << 6);
    }

    /* Bit 7: Square and sqrt check: 7.0*7.0==49.0, 49.0/7.0==7.0 */
    {
        volatile float a = 7.0f;
        float sq = a * a;
        float back = sq / a;
        if (sq == 49.0f && back == 7.0f) status |= (1 << 7);
    }

    return status; /* expect 0x00FF */
}
