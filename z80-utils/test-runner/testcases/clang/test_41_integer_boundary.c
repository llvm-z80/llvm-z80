/* Test 41: Integer boundary - overflow, wrapping, saturation */
typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short uint16_t;
typedef short int16_t;

uint16_t sat_add(uint16_t a, uint16_t b) {
    uint16_t r = a + b;
    if (r < a) return 0xFFFF; /* overflow detected */
    return r;
}

int main() {
    uint16_t status = 0;

    /* Bit 0: unsigned overflow: (uint8_t)(255+1)==0; (uint16_t)(65535+1)==0 */
    {
        volatile uint8_t a8 = 255;
        volatile uint8_t b8 = 1;
        uint8_t sum8 = a8 + b8;
        volatile uint16_t a16 = 65535;
        volatile uint16_t b16 = 1;
        uint16_t sum16 = a16 + b16;
        if (sum8 == 0 && sum16 == 0) status |= 1;
    }

    /* Bit 1: signed overflow behavior: (int8_t)127 + (int8_t)1 wraps; INT16_MIN-1 wraps */
    {
        volatile int8_t s8a = 127;
        volatile int8_t s8b = 1;
        int8_t sum8 = s8a + s8b; /* wraps to -128 */
        volatile int16_t s16a = -32768;
        volatile int16_t s16b = 1;
        int16_t diff16 = s16a - s16b; /* wraps to 32767 */
        if (sum8 == -128 && diff16 == 32767) status |= 2;
    }

    /* Bit 2: INT8_MIN negation: -(int8_t)-128 wraps to -128 */
    {
        volatile int8_t a = -128;
        int8_t neg = -a; /* wraps to -128 */
        if (neg == -128) status |= 4;
    }

    /* Bit 3: saturating add: sat_add(60000,10000)==65535; sat_add(100,200)==300 */
    {
        volatile uint16_t x1 = 60000, y1 = 10000;
        volatile uint16_t x2 = 100, y2 = 200;
        if (sat_add(x1, y1) == 65535 && sat_add(x2, y2) == 300)
            status |= 8;
    }

    return status; /* expect 0x000F = 15 */
}
