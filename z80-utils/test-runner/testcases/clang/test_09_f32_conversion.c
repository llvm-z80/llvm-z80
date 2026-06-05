/* Test 09: f32 conversion - int-to-float, float-to-int, round-trip */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned long uint32_t;
typedef long int32_t;

int main() {
    uint16_t status = 0;

    /* Bit 0: int-to-float: (float)42 == 42.0f; (float)(-10) == -10.0f */
    {
        volatile int16_t a = 42;
        volatile int16_t b = -10;
        volatile float fa = (float)a;
        volatile float fb = (float)b;
        if (fa == 42.0f && fb == -10.0f) status |= (1 << 0);
    }

    /* Bit 1: float-to-int: (int16_t)3.7f == 3; (int16_t)(-3.7f) == -3 */
    {
        volatile float a = 3.7f;
        volatile float b = -3.7f;
        int16_t ia = (int16_t)a;
        int16_t ib = (int16_t)b;
        if (ia == 3 && ib == -3) status |= (1 << 1);
    }

    /* Bit 2: unsigned to float: (float)(uint16_t)50000 == 50000.0f */
    {
        volatile uint16_t a = 50000;
        volatile float fa = (float)a;
        if (fa == 50000.0f) status |= (1 << 2);
    }

    /* Bit 3: round-trip: (int32_t)(float)(int32_t)12345 == 12345 */
    {
        volatile int32_t a = 12345L;
        volatile float f = (float)a;
        volatile int32_t b = (int32_t)f;
        if (b == 12345L) status |= (1 << 3);
    }

    /* Bit 4: i8 to float and back: (float)(int8_t)-100 == -100.0f */
    {
        volatile signed char a = -100;
        volatile float f = (float)a;
        signed char back = (signed char)f;
        if (f == -100.0f && back == -100) status |= (1 << 4);
    }

    /* Bit 5: uint32 to float: 1000000 -> 1000000.0f */
    {
        volatile uint32_t a = 1000000UL;
        volatile float f = (float)a;
        if (f == 1000000.0f) status |= (1 << 5);
    }

    /* Bit 6: Float truncation toward zero: 9.9f -> 9, -9.9f -> -9 */
    {
        volatile float a = 9.9f, b = -9.9f;
        int16_t ia = (int16_t)a;
        int16_t ib = (int16_t)b;
        if (ia == 9 && ib == -9) status |= (1 << 6);
    }

    /* Bit 7: Zero conversions: (float)0 == 0.0f, (int16_t)0.0f == 0 */
    {
        volatile int16_t a = 0;
        volatile float f = (float)a;
        int16_t back = (int16_t)f;
        if (f == 0.0f && back == 0) status |= (1 << 7);
    }

    return status; /* expect 0x00FF */
}
