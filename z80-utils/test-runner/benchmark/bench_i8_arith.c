/* Benchmark: i8 arithmetic */
typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short uint16_t;

int main() {
    uint16_t status = 0;

    /* Bit 0: Multiply chain: 3*7=21, 21*5=105 */
    {
        volatile uint8_t a = 3, b = 7, c = 5;
        uint8_t r = (uint8_t)(a * b);
        r = (uint8_t)(r * c);
        if (r == 105) status |= 1;
    }

    /* Bit 1: Unsigned div/mod: 253/10=25 rem 3, 200/7=28 rem 4 */
    {
        volatile uint8_t a = 253, b = 10, c = 200, d = 7;
        if (a / b == 25 && a % b == 3 && c / d == 28 && c % d == 4)
            status |= 2;
    }

    /* Bit 2: Signed arithmetic: (-50)+30=-20, (-10)*(-5)=50, (-100)/3=-33 */
    {
        volatile int8_t a = -50, b = 30, c = -10, d = -5, e = -100, f = 3;
        int8_t r1 = a + b;
        int8_t r2 = c * d;
        int8_t r3 = e / f;
        if (r1 == -20 && r2 == 50 && r3 == -33) status |= 4;
    }

    /* Bit 3: Shift and rotate: popcount of 0xA5, rotate left 3 */
    {
        volatile uint8_t val = 0xA5;
        uint8_t v = val;
        uint8_t count = 0;
        while (v) { count += (v & 1); v >>= 1; }
        uint8_t rotl = (uint8_t)((val << 3) | (val >> 5));
        /* 0xA5=10100101 -> 4 bits; rotl 3 = 0x2D */
        if (count == 4 && rotl == 0x2D) status |= 8;
    }

    /* Bit 4: Bitwise AND/OR/XOR */
    {
        volatile uint8_t a = 0xAC, b = 0x53;
        uint8_t x = a ^ b;  /* 0xFF */
        uint8_t y = a & b;  /* 0x00 */
        uint8_t z = a | b;  /* 0xFF */
        if (x == 0xFF && y == 0x00 && z == 0xFF) status |= 0x10;
    }

    /* Bit 5: Add/sub chain: 100+55-23+17-49 = 100 */
    {
        volatile uint8_t a = 100, b = 55, c = 23, d = 17, e = 49;
        uint8_t r = a + b;
        r = r - c;
        r = r + d;
        r = r - e;
        if (r == 100) status |= 0x20;
    }

    /* Bit 6: Loop accumulation with conditional: sum vals > 15 */
    {
        volatile uint8_t vals[4];
        vals[0] = 10; vals[1] = 20; vals[2] = 30; vals[3] = 40;
        uint8_t sum = 0;
        uint8_t i;
        for (i = 0; i < 4; i++) {
            if (vals[i] > 15) sum += vals[i];
        }
        /* 20+30+40 = 90 */
        if (sum == 90) status |= 0x40;
    }

    /* Bit 7: Unsigned overflow: 250+10=4, 3-10=249 */
    {
        volatile uint8_t a = 250, b = 10, c = 3;
        uint8_t sum = a + b;
        uint8_t diff = c - b;
        if (sum == 4 && diff == 249) status |= 0x80;
    }

    return status; /* expect 0x00FF */
}
