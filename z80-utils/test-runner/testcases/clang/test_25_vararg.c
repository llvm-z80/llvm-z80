/* Test 25: Variadic functions - sum with 0, 1, 2, 3 arguments */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned long uint32_t;
typedef long int32_t;

/* Variadic sum: first arg is count, then count int16_t values */
int16_t vsum(uint16_t count, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, count);
    int16_t total = 0;
    for (uint16_t i = 0; i < count; i++) {
        total += __builtin_va_arg(ap, int);
    }
    __builtin_va_end(ap);
    return total;
}

int main() {
    uint16_t status = 0;

    /* Bit 0: sum of 0 args -> 0 */
    {
        int16_t r = vsum(0);
        if (r == 0)
            status |= 1;
    }

    /* Bit 1: sum of 1 arg -> 42 */
    {
        volatile int16_t a = 42;
        int16_t r = vsum(1, (int)a);
        if (r == 42)
            status |= 2;
    }

    /* Bit 2: sum of 2 args -> 30 */
    {
        volatile int16_t a = 10, b = 20;
        int16_t r = vsum(2, (int)a, (int)b);
        if (r == 30)
            status |= 4;
    }

    /* Bit 3: sum of 3 args -> 600 */
    {
        volatile int16_t a = 100, b = 200, c = 300;
        int16_t r = vsum(3, (int)a, (int)b, (int)c);
        if (r == 600)
            status |= 8;
    }

    /* Bit 4: sum of 5 args -> 15 (1+2+3+4+5) */
    {
        int16_t r = vsum(5, 1, 2, 3, 4, 5);
        if (r == 15) status |= (1 << 4);
    }

    /* Bit 5: sum with negative values: -10 + 20 + -30 == -20 */
    {
        int16_t r = vsum(3, -10, 20, -30);
        if (r == -20) status |= (1 << 5);
    }

    /* Bit 6: sum of single large value */
    {
        int16_t r = vsum(1, 32000);
        if (r == 32000) status |= (1 << 6);
    }

    /* Bit 7: sum of 7 args: 10+20+30+40+50+60+70 == 280 */
    {
        int16_t r = vsum(7, 10, 20, 30, 40, 50, 60, 70);
        if (r == 280) status |= (1 << 7);
    }

    return status; /* expect 0x00FF */
}
