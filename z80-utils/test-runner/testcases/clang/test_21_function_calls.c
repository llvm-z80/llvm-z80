/* Test 21: Function calls - multi-arg, deeply nested, iterative GCD, min/max/clamp */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned long uint32_t;
typedef long int32_t;

uint16_t add3(uint16_t a, uint16_t b, uint16_t c) {
    return a + b + c;
}

uint16_t dot2d(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    return x1 * x2 + y1 * y2;
}

uint16_t h_func(uint16_t x) { return x + 1; }
uint16_t g_func(uint16_t x) { return x * 2; }
uint16_t f_func(uint16_t x) { return x + 10; }

uint16_t gcd(uint16_t a, uint16_t b) {
    while (b != 0) {
        uint16_t t = b;
        b = a % b;
        a = t;
    }
    return a;
}

int16_t min16(int16_t a, int16_t b) {
    return a < b ? a : b;
}

int16_t max16(int16_t a, int16_t b) {
    return a > b ? a : b;
}

int16_t clamp16(int16_t val, int16_t lo, int16_t hi) {
    return min16(max16(val, lo), hi);
}

int main() {
    uint16_t status = 0;

    /* Bit 0: 3-arg and 4-arg function calls */
    {
        volatile uint16_t a = 100, b = 200, c = 300;
        volatile uint16_t x1 = 3, y1 = 4, x2 = 5, y2 = 6;
        uint16_t sum = add3(a, b, c);         /* 600 */
        uint16_t dot = dot2d(x1, y1, x2, y2); /* 3*5 + 4*6 = 15+24 = 39 */
        if (sum == 600 && dot == 39)
            status |= 1;
    }

    /* Bit 1: deeply nested calls f(g(h(1))) = f(g(2)) = f(4) = 14 */
    {
        volatile uint16_t x = 1;
        uint16_t result = f_func(g_func(h_func(x)));
        /* h(1)=2, g(2)=4, f(4)=14 */
        if (result == 14)
            status |= 2;
    }

    /* Bit 2: iterative GCD */
    {
        volatile uint16_t a1 = 48, b1 = 18;
        volatile uint16_t a2 = 100, b2 = 75;
        if (gcd(a1, b1) == 6 && gcd(a2, b2) == 25)
            status |= 4;
    }

    /* Bit 3: nested min/max/clamp chains */
    {
        volatile int16_t v1 = 50;   /* within [10,30] -> clamp to 30 */
        volatile int16_t v2 = 5;    /* below [10,30] -> clamp to 10 */
        volatile int16_t v3 = 20;   /* within [10,30] -> 20 */
        volatile int16_t a = 15, b = 25;
        /* clamp(min(a,b), 10, 30) = clamp(15, 10, 30) = 15 */
        int16_t r1 = clamp16(min16(a, b), 10, 30);
        /* clamp(max(a,b), 10, 20) = clamp(25, 10, 20) = 20 */
        int16_t r2 = clamp16(max16(a, b), 10, 20);
        /* clamp(v1, 10, 30) = 30 */
        int16_t r3 = clamp16(v1, 10, 30);
        /* clamp(v2, 10, 30) = 10 */
        int16_t r4 = clamp16(v2, 10, 30);
        if (r1 == 15 && r2 == 20 && r3 == 30 && r4 == 10)
            status |= 8;
    }

    return status; /* expect 0x000F = 15 */
}
