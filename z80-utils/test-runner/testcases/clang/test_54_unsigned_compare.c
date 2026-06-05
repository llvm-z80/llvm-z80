/* Test 54: Unsigned comparison patterns that may trigger legalizer issues */
/* Tests i > 0, i < max, and other unsigned boundary comparisons */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

int main() {
    uint16_t status = 0;

    /* Bit 0: while (i > 0) countdown with u16 */
    {
        volatile uint16_t n = 10;
        uint16_t sum = 0;
        uint16_t i = n;
        while (i > 0) {
            sum += i;
            i--;
        }
        /* 1+2+...+10 = 55 */
        if (sum == 55) status |= (1 << 0);
    }

    /* Bit 1: while (i > 0) with u8 */
    {
        volatile uint8_t n = 5;
        uint8_t prod = 1;
        uint8_t i = n;
        while (i > 0) {
            prod *= i;
            i--;
        }
        /* 5! = 120 */
        if (prod == 120) status |= (1 << 1);
    }

    /* Bit 2: for loop with unsigned > comparison */
    {
        volatile uint16_t limit = 3;
        uint16_t count = 0;
        uint16_t i;
        for (i = 10; i > limit; i--) count++;
        /* counts: 10,9,8,7,6,5,4 = 7 */
        if (count == 7) status |= (1 << 2);
    }

    /* Bit 3: unsigned less-than boundary */
    {
        volatile uint16_t a = 0, b = 1, c = 65535u;
        uint8_t r = 0;
        if (a < b) r |= 1;
        if (b < c) r |= 2;
        if (!(c < a)) r |= 4;
        if (r == 7) status |= (1 << 3);
    }

    /* Bit 4: unsigned >= and <= */
    {
        volatile uint16_t x = 100;
        uint8_t r = 0;
        if (x >= 100) r |= 1;
        if (x <= 100) r |= 2;
        if (x >= 0) r |= 4;
        if (r == 7) status |= (1 << 4);
    }

    /* Bit 5: Accumulate while u16 > threshold */
    {
        volatile uint16_t start = 1000;
        uint16_t v = start;
        uint8_t steps = 0;
        while (v > 100) {
            v = v / 2;
            steps++;
        }
        /* 1000,500,250,125,62 -> 4 steps (62 <= 100) */
        /* Actually: 1000/2=500, 500/2=250, 250/2=125, 125/2=62 -> steps=4 */
        if (steps == 4 && v == 62) status |= (1 << 5);
    }

    /* Bit 6: Mixed u8 comparisons in loop */
    {
        volatile uint8_t arr[5];
        arr[0] = 10; arr[1] = 3; arr[2] = 255; arr[3] = 0; arr[4] = 128;
        uint8_t max = 0;
        uint8_t i;
        for (i = 0; i < 5; i++) {
            if (arr[i] > max) max = arr[i];
        }
        if (max == 255) status |= (1 << 6);
    }

    /* Bit 7: do-while with unsigned > 0 exit */
    {
        volatile uint16_t n = 256;
        uint16_t v = n;
        uint8_t bits = 0;
        do {
            v >>= 1;
            bits++;
        } while (v > 0);
        /* 256>>1=128, >>1=64, ..., >>1=1, >>1=0 -> 9 shifts */
        if (bits == 9) status |= (1 << 7);
    }

    return status; /* expect 0x00FF */
}
