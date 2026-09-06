/* Test 07: i64 loops - factorial, power, sum */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long long uint64_t;
typedef long long int64_t;

/* Factorial with i64 result */
uint64_t factorial(uint8_t n) {
    uint64_t r = 1;
    for (uint8_t i = 2; i <= n; i++)
        r *= i;
    return r;
}

/* Power function with i64 */
uint64_t power(uint16_t base, uint8_t exp) {
    uint64_t r = 1;
    for (uint8_t i = 0; i < exp; i++)
        r *= base;
    return r;
}

int main() {
    uint16_t status = 0;

    /* Bit 0: factorial(10) = 3628800 */
    {
        if (factorial(10) == 3628800ULL) status |= (1 << 0);
    }

    /* Bit 1: factorial(15) = 1307674368000 */
    {
        if (factorial(15) == 1307674368000ULL) status |= (1 << 1);
    }

    /* Bit 2: power(10, 9) = 1000000000 */
    {
        if (power(10, 9) == 1000000000ULL) status |= (1 << 2);
    }

    /* Bit 3: sum 1..100 using i64 accumulator == 5050 */
    {
        volatile int64_t n = 100LL;
        int64_t sum = 0;
        while (n > 0) {
            sum += n;
            n--;
        }
        if (sum == 5050LL) status |= (1 << 3);
    }

    /* Bit 4: power(2, 32) == 4294967296 (crosses 32-bit boundary) */
    {
        if (power(2, 32) == 4294967296ULL) status |= (1 << 4);
    }

    /* Bit 5: factorial(0) == 1, factorial(1) == 1 (edge cases) */
    {
        if (factorial(0) == 1 && factorial(1) == 1) status |= (1 << 5);
    }

    /* Bit 6: Fibonacci(30) via i64 loop == 832040 */
    {
        uint64_t a = 0, b = 1;
        for (uint8_t i = 0; i < 30; i++) {
            uint64_t t = a + b;
            a = b;
            b = t;
        }
        if (a == 832040ULL) status |= (1 << 6);
    }

    /* Bit 7: Countdown with i64: start at 1000000, subtract 999 each step, 1001 steps */
    {
        volatile uint64_t val = 1000000ULL;
        for (uint16_t i = 0; i < 1001; i++) val -= 999;
        if (val == 1ULL) status |= (1 << 7);  /* 1000000 - 999*1001 = 1000000 - 999999 = 1 */
    }

    return status; /* expect 0x00FF */
}
