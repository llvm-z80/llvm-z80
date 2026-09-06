/* Test 13: shift operations - identity, mul/div, boundary, variable, 8-bit, signed */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef short int16_t;

int main() {
    uint16_t status = 0;

    /* Bit 0: shift by 0 is identity: x<<0==x, x>>0==x for several values */
    {
        volatile uint16_t a = 0x1234;
        volatile uint16_t b = 0xABCD;
        volatile uint16_t c = 0x0001;
        if ((a << 0) == 0x1234 && (a >> 0) == 0x1234 &&
            (b << 0) == 0xABCD && (b >> 0) == 0xABCD &&
            (c << 0) == 0x0001 && (c >> 0) == 0x0001)
            status |= (1 << 0);
    }

    /* Bit 1: shift by 1 is mul/div by 2: 100<<1==200, 100>>1==50 */
    {
        volatile uint16_t a = 100;
        if ((a << 1) == 200 && (a >> 1) == 50)
            status |= (1 << 1);
    }

    /* Bit 2: shift by (width-1): 1<<15==0x8000 (i16), (uint16_t)0x8000>>15==1 */
    {
        volatile uint16_t a = 1;
        volatile uint16_t b = 0x8000;
        if ((a << 15) == 0x8000 && (b >> 15) == 1)
            status |= (1 << 2);
    }

    /* Bit 3: variable shift loop: shift 1 left by 0..15, verify each is 1<<i */
    {
        volatile uint16_t val = 1;
        uint8_t ok = 1;
        for (uint8_t i = 0; i < 16; i++) {
            if ((val << i) != ((uint16_t)1 << i)) ok = 0;
        }
        if (ok) status |= (1 << 3);
    }

    /* Bit 4: 8-bit shifts: 0x81<<1==0x02 (wraps), 0x81>>1==0x40 */
    {
        volatile uint8_t a = 0x81;
        uint8_t l = (uint8_t)(a << 1);
        uint8_t r = a >> 1;
        if (l == 0x02 && r == 0x40) status |= (1 << 4);
    }

    /* Bit 5: Shift as multiply/divide by power of 2: 13<<3==104, 104>>3==13 */
    {
        volatile uint16_t a = 13;
        if ((a << 3) == 104 && (104 >> 3) == 13) status |= (1 << 5);
    }

    /* Bit 6: Signed right shift preserves sign: (int16_t)-128 >> 2 == -32 */
    {
        volatile int16_t a = -128;
        int16_t r = a >> 2;
        if (r == -32) status |= (1 << 6);
    }

    /* Bit 7: Shift by 8: move bytes around: 0x1234<<8==0x3400, 0x1234>>8==0x12 */
    {
        volatile uint16_t a = 0x1234;
        uint16_t l = a << 8;
        uint16_t r = a >> 8;
        if (l == 0x3400 && r == 0x0012) status |= (1 << 7);
    }

    return status; /* expect 0x00FF */
}
