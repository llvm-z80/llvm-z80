/* Test 52: Loop patterns - while, do-while, for, nested, countdown, early exit */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef short int16_t;

int main() {
    uint16_t status = 0;

    /* Bit 0: While loop with condition change: count leading zeros of 0x0800 == 4 */
    {
        volatile uint16_t val = 0x0800;
        uint8_t clz = 0;
        uint16_t v = val;
        while (v != 0 && !(v & 0x8000)) { clz++; v <<= 1; }
        if (clz == 4) status |= (1 << 0);
    }

    /* Bit 1: Do-while at least once: start at 100, divide by 3 until < 5 */
    {
        volatile uint16_t val = 100;
        uint8_t steps = 0;
        uint16_t v = val;
        do { v /= 3; steps++; } while (v >= 5);
        /* 100/3=33, 33/3=11, 11/3=3 -> 3 steps */
        if (steps == 3 && v == 3) status |= (1 << 1);
    }

    /* Bit 2: Nested for loops: multiply 3x4 matrix elements by index */
    {
        uint16_t sum = 0;
        for (uint8_t i = 0; i < 3; i++)
            for (uint8_t j = 0; j < 4; j++)
                sum += (i + 1) * (j + 1);
        /* sum = (1+2+3)*(1+2+3+4) = 6*10 = 60 */
        if (sum == 60) status |= (1 << 2);
    }

    /* Bit 3: For loop with continue: sum only even numbers 0..19 */
    {
        uint16_t sum = 0;
        for (uint8_t i = 0; i < 20; i++) {
            if (i & 1) continue;
            sum += i;
        }
        /* 0+2+4+6+8+10+12+14+16+18 = 90 */
        if (sum == 90) status |= (1 << 3);
    }

    /* Bit 4: For loop with break: find first multiple of 7 > 50 */
    {
        uint8_t found = 0;
        for (uint8_t i = 1; i < 100; i++) {
            if (i % 7 == 0 && i > 50) { found = i; break; }
        }
        if (found == 56) status |= (1 << 4);
    }

    /* Bit 5: Countdown loop: 255 down to 0 (uint8_t wrapping) */
    {
        volatile uint8_t count = 0;
        for (uint8_t i = 255; i != 0; i--) count++;
        if (count == 255) status |= (1 << 5);
    }

    /* Bit 6: GCD via Euclidean algorithm: gcd(48, 18) == 6 */
    {
        volatile uint16_t a = 48, b = 18;
        uint16_t x = a, y = b;
        while (y != 0) {
            uint16_t t = y;
            y = x % y;
            x = t;
        }
        if (x == 6) status |= (1 << 6);
    }

    /* Bit 7: Triple nested loop: 5*4*3 == 60 iterations counted */
    {
        uint16_t count = 0;
        for (uint8_t i = 0; i < 5; i++)
            for (uint8_t j = 0; j < 4; j++)
                for (uint8_t k = 0; k < 3; k++)
                    count++;
        if (count == 60) status |= (1 << 7);
    }

    return status; /* expect 0x00FF */
}
